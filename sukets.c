/* ================================================================
 *  sukets.c — Libreria de sockets simple para Itsuki
 *  v2.2.0 — Multiplataforma (Windows / Linux)
 *
 *  Contenido:
 *    1.  Ciclo de vida
 *    2.  Errores unificados
 *    3.  Utilidades internas
 *    4.  TCP cliente / servidor
 *    5.  Envio y recepcion
 *    6.  Opciones de socket
 *    7.  Multiplexacion (select)
 *    8.  UDP
 *    9.  Sockets raw
 * ================================================================ */

#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>

#ifdef _WIN32
  #include      <winsock2.h>
  #include      <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef SOCKET sukets_sock_t;
  #define SUKETS_INVALID        INVALID_SOCKET
  #define SUKETS_ERROR          SOCKET_ERROR
  #define SUKETS_CLOSE(s)       closesocket(s)
  #define SUKETS_NATIVE_ERR()   ((int)WSAGetLastError())
#else
  #include      <sys/types.h>
  #include      <sys/socket.h>
  #include      <sys/select.h>
  #include      <netinet/in.h>
  #include      <netinet/tcp.h>
  #include      <netinet/ip.h>
  #include      <arpa/inet.h>
  #include      <netdb.h>
  #include      <unistd.h>
  #include      <errno.h>
  typedef int   sukets_sock_t;
  #define SUKETS_INVALID        (-1)
  #define SUKETS_ERROR          (-1)
  #define SUKETS_CLOSE(s)       close(s)
  #define SUKETS_NATIVE_ERR()   (errno)
#endif

/* ================================================================
 *  1. Ciclo de vida
 * ================================================================ */

static int      sk_ready = 0;

int
sk_init(void)
{
#ifdef _WIN32
        WSADATA wsaData;
        if (!sk_ready) {
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                        return -1;
                sk_ready = 1;
        }
#endif
        return 0;
}

void
sk_cleanup(void)
{
#ifdef _WIN32
        if (sk_ready) {
                WSACleanup();
                sk_ready = 0;
        }
#endif
}

/* ================================================================
 *  2. Errores unificados
 *
 *  sk_last_error()  devuelve un codigo de SUKETS (portable).
 *  sk_native_error() devuelve el codigo crudo del OS (debug).
 * ================================================================ */

int
sk_native_error(void)
{
        return SUKETS_NATIVE_ERR();
}

int
sk_last_error(void)
{
        int e = SUKETS_NATIVE_ERR();

        switch (e) {
#ifdef _WIN32
        case WSAEWOULDBLOCK:  return 1;   /* SK_E_WOULD_BLOCK  */
        case WSAECONNRESET:   return 2;   /* SK_E_CONN_RESET   */
        case WSAECONNREFUSED:  return 3;  /* SK_E_CONN_REFUSED */
        case WSAETIMEDOUT:     return 4;  /* SK_E_TIMED_OUT    */
        case WSAENETUNREACH:   return 5;  /* SK_E_UNREACH      */
        case WSAEINPROGRESS:   return 6;  /* SK_E_IN_PROGRESS  */
        case WSAEACCES:        return 8;  /* SK_E_ACCESS       */
#else
        case EAGAIN:
        case EWOULDBLOCK:     return 1;
        case ECONNRESET:       return 2;
        case ECONNREFUSED:     return 3;
        case ETIMEDOUT:        return 4;
        case ENETUNREACH:      return 5;
        case EINPROGRESS:      return 6;
        case EACCES:
        case EPERM:            return 8;
#endif
        default:               return -1; /* desconocido: ver sk_native_error() */
        }
}

/* ================================================================
 *  3. Utilidades internas
 * ================================================================ */

/* sockaddr_in IPv4 -> texto. No usa inet_ntop para no depender
   de la version de headers de MinGW. */
static int
sk_ip_to_str(const struct sockaddr_in *addr, char *buf, int buf_len)
{
        unsigned char *b = (unsigned char *)&addr->sin_addr;

        if (buf_len < 16)
                return -1;
        sprintf(buf, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        return 0;
}

/* Crea un addrinfo. Acepta IPs y nombres DNS.
   udp: 0 = SOCK_STREAM, 1 = SOCK_DGRAM, 2 = SOCK_RAW. */
static int
sk_resolve(const char *host, const char *port, int udp, struct addrinfo **res)
{
        struct addrinfo hints;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = (udp == 1) ? SOCK_DGRAM : SOCK_STREAM;
        if (udp == 2)
                hints.ai_socktype = SOCK_RAW;

        if (getaddrinfo(host, port, &hints, res) != 0)
                return -1;
        return 0;
}

/* ================================================================
 *  4. TCP cliente / servidor
 * ================================================================ */

/* Devuelve el descriptor o: -1 init, -2 socket, -3 DNS, -4 connect. */
int
sk_tcp_connect(const char *host, int port)
{
        struct addrinfo *res = NULL, *rp;
        char port_str[12];
        sukets_sock_t sock;

        if (sk_init() != 0)
                return -1;

        sprintf(port_str, "%d", port);
        if (sk_resolve(host, port_str, 0, &res) != 0)
                return -3;

        for (rp = res; rp != NULL; rp = rp->ai_next) {
                sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (sock == SUKETS_INVALID)
                        continue;
                if (connect(sock, rp->ai_addr, (int)rp->ai_addrlen) != SUKETS_ERROR)
                        break;
                SUKETS_CLOSE(sock);
                sock = SUKETS_INVALID;
        }

        freeaddrinfo(res);

        if (rp == NULL || sock == SUKETS_INVALID)
                return -4;

        return (int)(long)sock;
}

/* host: IP local o NULL para escuchar en todas las interfaces.
   Devuelve el socket en escucha o: -1 init, -2 socket, -3 bind, -4 listen. */
int
sk_tcp_server(const char *host, int port, int backlog)
{
        struct addrinfo hints, *res = NULL, *rp;
        char port_str[12];
        sukets_sock_t sock = SUKETS_INVALID;
        int opt = 1;

        if (sk_init() != 0)
                return -1;

        sprintf(port_str, "%d", port);
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(host, port_str, &hints, &res) != 0)
                return -3;

        for (rp = res; rp != NULL; rp = rp->ai_next) {
                sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (sock == SUKETS_INVALID)
                        continue;
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                           (const char *)&opt, sizeof(opt));
                if (bind(sock, rp->ai_addr, (int)rp->ai_addrlen) != SUKETS_ERROR)
                        break;
                SUKETS_CLOSE(sock);
                sock = SUKETS_INVALID;
        }

        freeaddrinfo(res);

        if (rp == NULL || sock == SUKETS_INVALID)
                return -3;

        if (listen(sock, backlog) == SUKETS_ERROR) {
                SUKETS_CLOSE(sock);
                return -4;
        }

        return (int)(long)sock;
}

int
sk_accept(int server_sock)
{
        sukets_sock_t client = accept((sukets_sock_t)server_sock, NULL, NULL);

        if (client == SUKETS_INVALID)
                return -1;
        return (int)(long)client;
}

/* Cierra solo un sentido (o ambos) sin liberar el descriptor.
   how: 0 = dejar de recibir, 1 = dejar de enviar, 2 = ambos. */
int
sk_shutdown(int sock, int how)
{
#ifdef _WIN32
        int h = (how == 0) ? SD_RECEIVE : (how == 1) ? SD_SEND : SD_BOTH;
#else
        int h = (how == 0) ? SHUT_RD : (how == 1) ? SHUT_WR : SHUT_RDWR;
#endif
        if (shutdown((sukets_sock_t)sock, h) == SUKETS_ERROR)
                return -1;
        return 0;
}

int
sk_close(int sock)
{
        return SUKETS_CLOSE((sukets_sock_t)sock);
}

/* ================================================================
 *  5. Envio y recepcion
 *
 *  sk_send / sk_recv: semantica clasica (pueden procesar menos
 *  bytes de los pedidos). sk_send_all / sk_recv_all garantizan el
 *  total (recv_all agrega '\0').
 *  Devuelven bytes procesados, 0 en cierre del peer, o -1 en error.
 * ================================================================ */

int
sk_send(int sock, const char *data, int len)
{
        return send((sukets_sock_t)sock, data, len, 0);
}

int
sk_recv(int sock, char *buffer, int max_len)
{
        return recv((sukets_sock_t)sock, buffer, max_len, 0);
}

int
sk_send_all(int sock, const char *data, int len)
{
        int total = 0;

        while (total < len) {
                int n = send((sukets_sock_t)sock, data + total, len - total, 0);
                if (n == SUKETS_ERROR)
                        return -1;
                if (n == 0)
                        break;
                total += n;
        }
        return total;
}

int
sk_recv_all(int sock, char *buffer, int max_len)
{
        int total = 0;

        while (total < max_len - 1) {
                int n = recv((sukets_sock_t)sock, buffer + total, max_len - 1 - total, 0);
                if (n == SUKETS_ERROR)
                        return -1;
                if (n == 0)
                        break;
                total += n;
        }
        buffer[total] = '\0';
        return total;
}

/* ================================================================
 *  6. Opciones de socket
 * ================================================================ */

/* Timeout de recv y send en milisegundos. 0 = bloqueante. */
int
sk_set_timeout(int sock, int milliseconds)
{
#ifdef _WIN32
        DWORD timeout = (DWORD)milliseconds;
#else
        struct timeval timeout;
        timeout.tv_sec = milliseconds / 1000;
        timeout.tv_usec = (milliseconds % 1000) * 1000;
#endif
        if (setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_RCVTIMEO,
                       (const char *)&timeout, sizeof(timeout)) == SUKETS_ERROR)
                return -1;
        if (setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_SNDTIMEO,
                       (const char *)&timeout, sizeof(timeout)) == SUKETS_ERROR)
                return -1;
        return 0;
}

/* 1 desactiva el algoritmo de Nagle: mensajeria de paquetes cortos. */
int
sk_set_nodelay(int sock, int enabled)
{
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, IPPROTO_TCP, TCP_NODELAY,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
}

/* 1 habilita la deteccion de peers muertos a nivel TCP. */
int
sk_set_keepalive(int sock, int enabled)
{
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_KEEPALIVE,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
}

/* 1 permite reusar una direccion local en TIME_WAIT. */
int
sk_set_reuseaddr(int sock, int enabled)
{
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_REUSEADDR,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
}

/* IP del otro extremo de la conexion TCP. */
int
sk_peer_ip(int sock, char *ip_buf, int ip_len)
{
        struct sockaddr_in peer;
        int peer_len = sizeof(peer);

        if (getpeername((sukets_sock_t)sock, (struct sockaddr *)&peer, &peer_len) == SUKETS_ERROR)
                return -1;
        return sk_ip_to_str(&peer, ip_buf, ip_len);
}

/* Puerto local del socket (ej. tras udp_bind con puerto 0). */
int
sk_local_port(int sock)
{
        struct sockaddr_in addr;
        int len = sizeof(addr);

        if (getsockname((sukets_sock_t)sock, (struct sockaddr *)&addr, &len) == SUKETS_ERROR)
                return -1;
        return (int)ntohs(addr.sin_port);
}

/* ================================================================
 *  7. Multiplexacion
 *
 *  Espera hasta que alguno de los sockets tenga datos listos.
 *  Devuelve el INDICE en socks[] del primero listo, -2 si vencio
 *  el timeout o -1 en error. count no puede superar FD_SETSIZE.
 * ================================================================ */

int
sk_select(const int *socks, int count, int timeout_ms)
{
        fd_set set;
        struct timeval tv;
        struct timeval *tvp = NULL;
        int i, max_fd = -1;

        if (count <= 0 || count > FD_SETSIZE)
                return -1;

        FD_ZERO(&set);
        for (i = 0; i < count; i++) {
                int fd = socks[i];
                if (fd < 0)
                        return -1;
                FD_SET((sukets_sock_t)fd, &set);
                if (fd > max_fd)
                        max_fd = fd;
        }

        if (timeout_ms >= 0) {
                tv.tv_sec = timeout_ms / 1000;
                tv.tv_usec = (timeout_ms % 1000) * 1000;
                tvp = &tv;
        }

        if (select(max_fd + 1, &set, NULL, NULL, tvp) == SUKETS_ERROR)
                return -1;

        for (i = 0; i < count; i++) {
                if (FD_ISSET((sukets_sock_t)socks[i], &set))
                        return i;
        }
        return -2;
}

/* ================================================================
 *  8. UDP
 * ================================================================ */

int
sk_udp_open(void)
{
        sukets_sock_t sock;

        if (sk_init() != 0)
                return -1;

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == SUKETS_INVALID)
                return -2;
        return (int)(long)sock;
}

/* host: IP local o NULL para todas las interfaces. port 0 = efimero. */
int
sk_udp_bind(int sock, const char *host, int port)
{
        struct sockaddr_in addr;
        int opt = 1;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short)port);
        addr.sin_addr.s_addr =
                (host != NULL) ? inet_addr(host) : htonl(INADDR_ANY);

        if (addr.sin_addr.s_addr == INADDR_NONE && host != NULL)
                return -3;

        setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_REUSEADDR,
                   (const char *)&opt, sizeof(opt));

        if (bind((sukets_sock_t)sock, (struct sockaddr *)&addr, sizeof(addr)) == SUKETS_ERROR)
                return -4;
        return 0;
}

int
sk_udp_sendto(int sock, const char *host, int port, const char *data, int len)
{
        struct addrinfo *res = NULL;
        char port_str[12];
        int sent;

        sprintf(port_str, "%d", port);
        if (sk_resolve(host, port_str, 1, &res) != 0)
                return -3;

        sent = (int)sendto((sukets_sock_t)sock, data, len, 0,
                           res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);
        return sent;
}

/* src_ip / src_port son opcionales (NULL / 0). */
int
sk_udp_recvfrom(int sock, char *buffer, int max_len,
                char *src_ip, int *src_port)
{
        struct sockaddr_in from;
        int from_len = sizeof(from);
        int n = (int)recvfrom((sukets_sock_t)sock, buffer, max_len, 0,
                              (struct sockaddr *)&from, &from_len);

        if (n == SUKETS_ERROR)
                return -1;
        buffer[n] = '\0';
        if (src_ip != NULL)
                sk_ip_to_str(&from, src_ip, 46);
        if (src_port != NULL)
                *src_port = (int)ntohs(from.sin_port);
        return n;
}

int
sk_udp_broadcast(int sock, int enabled)
{
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_BROADCAST,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
}

int
sk_udp_close(int sock)
{
        return SUKETS_CLOSE((sukets_sock_t)sock);
}

/* ================================================================
 *  9. Sockets raw
 *
 *  PROTO_ICMP=1, PROTO_TCP=6, PROTO_UDP=17 (numeros IP estandar).
 *  Requiere privilegios: root o CAP_NET_RAW en Linux; admin en
 *  Windows (y alla el stack limita lo que puede hacer).
 * ================================================================ */

int
sk_raw_open(int protocol)
{
        sukets_sock_t sock;

        if (sk_init() != 0)
                return -1;

        sock = socket(AF_INET, SOCK_RAW, protocol);
        if (sock == SUKETS_INVALID)
                return -2;
        return (int)(long)sock;
}

/* 1: el kernel no agrega la cabecera IP. Solo Linux. */
int
sk_raw_include_header(int sock, int enabled)
{
#ifdef _WIN32
        (void)sock;
        (void)enabled;
        return -9;   /* no soportado en Windows */
#else
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, IPPROTO_IP, IP_HDRINCL,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
#endif
}

int
sk_raw_sendto(int sock, const char *host, int protocol,
              const char *data, int len)
{
        struct addrinfo *res = NULL;
        char port_str[12];
        int sent;

        sprintf(port_str, "%d", protocol);
        if (sk_resolve(host, port_str, 2, &res) != 0)
                return -3;

        sent = (int)sendto((sukets_sock_t)sock, data, len, 0,
                           res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);
        return sent;
}

int
sk_raw_recvfrom(int sock, char *buffer, int max_len, char *src_ip)
{
        struct sockaddr_in from;
        int from_len = sizeof(from);
        int n = (int)recvfrom((sukets_sock_t)sock, buffer, max_len, 0,
                              (struct sockaddr *)&from, &from_len);

        if (n == SUKETS_ERROR)
                return -1;
        if (src_ip != NULL)
                sk_ip_to_str(&from, src_ip, 46);
        return n;
}

int
sk_raw_close(int sock)
{
        return SUKETS_CLOSE((sukets_sock_t)sock);
}

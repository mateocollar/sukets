/*
        sukets.c — Libreria de sockets simple para Itsuki
        v2.0.0 — Multiplataforma (Windows / Linux)

        TCP cliente, TCP servidor, UDP, resolucion DNS,
        timeouts, TCP_NODELAY, envio/recepcion completos.
*/

#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>

#ifdef _WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #include      <winsock2.h>
  #include      <ws2tcpip.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
  #endif
  typedef SOCKET sukets_sock_t;
  #define SUKETS_INVALID        INVALID_SOCKET
  #define SUKETS_ERROR          SOCKET_ERROR
  #define SUKETS_CLOSE(s)       closesocket(s)
  #define SUKETS_GETERR()       ((int)WSAGetLastError())
#else
  #include      <sys/types.h>
  #include      <sys/socket.h>
  #include      <netinet/in.h>
  #include      <netinet/tcp.h>
  #include      <arpa/inet.h>
  #include      <netdb.h>
  #include      <unistd.h>
  #include      <errno.h>
  #include      <fcntl.h>
  typedef int   sukets_sock_t;
  #define SUKETS_INVALID        (-1)
  #define SUKETS_ERROR          (-1)
  #define SUKETS_CLOSE(s)       close(s)
  #define SUKETS_GETERR()       (errno)
#endif

static int      sukets_wsa_ready = 0;

/*
        Ciclo de vida
        Llamar a sukets_init() una vez al comenzar y
        sukets_cleanup() al terminar. Es seguro llamarlas
        mas de una vez.
*/

int
sukets_init(void)
{
#ifdef _WIN32
        WSADATA wsaData;
        if (!sukets_wsa_ready) {
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                        return -1;
                sukets_wsa_ready = 1;
        }
#endif
        return 0;
}

void
sukets_cleanup(void)
{
#ifdef _WIN32
        if (sukets_wsa_ready) {
                WSACleanup();
                sukets_wsa_ready = 0;
        }
#endif
}

int
sukets_get_last_error(void)
{
        return SUKETS_GETERR();
}

/*
        Utilidad interna: convierte un sockaddr_in IPv4 a texto
        ("192.168.0.1"). Evita inet_ntop/inet_ntoa para no
        depender de la version de headers de MinGW.
        Devuelve 0 en exito, -1 si el buffer es muy chico.
*/

static int
sukets_ip_to_str(const struct sockaddr_in *addr, char *buf, int buf_len)
{
        unsigned char *b = (unsigned char *)&addr->sin_addr;

        if (buf_len < 16)
                return -1;
        sprintf(buf, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        return 0;
}

/*
        Utilidad interna: crea un addrinfo a partir de host y puerto.
        Acepta IPs ("127.0.0.1") y nombres DNS ("localhost", "example.com").
        Devuelve 0 y *res en exito; -1 en error de resolucion.
*/

static int
sukets_resolve(const char *host, const char *port, int udp, struct addrinfo **res)
{
        struct addrinfo hints;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = udp ? SOCK_DGRAM : SOCK_STREAM;

        if (getaddrinfo(host, port, &hints, res) != 0)
                return -1;
        return 0;
}

/*
        TCP — Cliente
        sukets_connect: devuelve el descriptor o:
          -1 init fallo
          -2 no se pudo crear el socket
          -3 no se pudo resolver el host
          -4 no se pudo conectar (ninguna direccion funciono)
*/

int
sukets_connect(const char *host, int port)
{
        struct addrinfo *res = NULL, *rp;
        char port_str[12];
        sukets_sock_t sock;

        if (sukets_init() != 0)
                return -1;

        sprintf(port_str, "%d", port);
        if (sukets_resolve(host, port_str, 0, &res) != 0)
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

/*
        TCP — Servidor
        sukets_server_create(host, port, backlog)
        host: IP local o NULL para escuchar en todas las interfaces.
        Devuelve el socket en escucha o:
          -1 init fallo   -2 socket   -3 resolve/bind   -4 listen
*/

int
sukets_server_create(const char *host, int port, int backlog)
{
        struct addrinfo hints, *res = NULL, *rp;
        char port_str[12];
        sukets_sock_t sock = SUKETS_INVALID;
        int opt = 1;

        if (sukets_init() != 0)
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
sukets_accept(int server_sock)
{
        sukets_sock_t client = accept((sukets_sock_t)server_sock, NULL, NULL);

        if (client == SUKETS_INVALID)
                return -1;
        return (int)(long)client;
}

/*
        Envio y recepcion
        sukets_send / sukets_recv mantienen la semantica clasica
        (pueden enviar/recibir menos bytes de los pedidos).

        sukets_send_all / sukets_recv_all garantizan la operacion
        completa (recv_all ademas agrega '\0' al final).
        Devuelven el total procesado, 0 en cierre del peer,
        o -1 en error.
*/

int
sukets_send(int sock, const char *data, int len)
{
        return send((sukets_sock_t)sock, data, len, 0);
}

int
sukets_recv(int sock, char *buffer, int max_len)
{
        return recv((sukets_sock_t)sock, buffer, max_len, 0);
}

int
sukets_send_all(int sock, const char *data, int len)
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
sukets_recv_all(int sock, char *buffer, int max_len)
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

/*
        Opciones de socket
        set_timeout: en milisegundos, aplica a recv y send. 0 = bloqueante.
        set_nodelay: 1 desactiva el algoritmo de Nagle (mensajes cortos).
        peer_ip: escribe en ip_buf la IP del otro extremo.
*/

int
sukets_set_timeout(int sock, int milliseconds)
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

int
sukets_set_nodelay(int sock, int enabled)
{
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, IPPROTO_TCP, TCP_NODELAY,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
}

int
sukets_peer_ip(int sock, char *ip_buf, int ip_len)
{
        struct sockaddr_in peer;
        int peer_len = sizeof(peer);

        if (getpeername((sukets_sock_t)sock, (struct sockaddr *)&peer, &peer_len) == SUKETS_ERROR)
                return -1;
        return sukets_ip_to_str(&peer, ip_buf, ip_len);
}

int
sukets_close(int sock)
{
        return SUKETS_CLOSE((sukets_sock_t)sock);
}

/*
        UDP
        udp_open:       crea un socket UDP no conectado.
        udp_bind:       lo liga a un puerto local (para recibir).
                        host: IP local o NULL para todas las interfaces.
        udp_sendto:     envia un datagrama a host:port.
        udp_recvfrom:   recibe; escribe la IP/puerto del remitente
                        en src_ip / src_port (opcionales: pasar NULL).
        udp_broadcast:  1 habilita envio broadcast (255.255.255.255).
*/

int
sukets_udp_open(void)
{
        sukets_sock_t sock;

        if (sukets_init() != 0)
                return -1;

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == SUKETS_INVALID)
                return -2;
        return (int)(long)sock;
}

int
sukets_udp_bind(int sock, const char *host, int port)
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
sukets_udp_sendto(int sock, const char *host, int port, const char *data, int len)
{
        struct addrinfo *res = NULL;
        char port_str[12];
        int sent;

        sprintf(port_str, "%d", port);
        if (sukets_resolve(host, port_str, 1, &res) != 0)
                return -3;

        sent = (int)sendto((sukets_sock_t)sock, data, len, 0,
                           res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);
        return sent;
}

int
sukets_udp_recvfrom(int sock, char *buffer, int max_len,
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
                sukets_ip_to_str(&from, src_ip, 46);
        if (src_port != NULL)
                *src_port = (int)ntohs(from.sin_port);
        return n;
}

int
sukets_udp_broadcast(int sock, int enabled)
{
        int flag = enabled ? 1 : 0;

        if (setsockopt((sukets_sock_t)sock, SOL_SOCKET, SO_BROADCAST,
                       (const char *)&flag, sizeof(flag)) == SUKETS_ERROR)
                return -1;
        return 0;
}

int
sukets_udp_close(int sock)
{
        return SUKETS_CLOSE((sukets_sock_t)sock);
}

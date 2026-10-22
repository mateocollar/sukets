# sukets

Librería de sockets simple y multiplataforma para [itsuki-lang](https://github.com/Apolo1061/itsuki-lang).

![Versión](https://img.shields.io/badge/versi%C3%B3n-2.2.0-blue)
![Estado](https://img.shields.io/badge/estado-experimental-orange)
![Plataformas](https://img.shields.io/badge/plataformas-Windows%20%7C%20Linux-lightgrey)
![Arquitectura](https://img.shields.io/badge/arquitectura-x86__32%20%7C%20x86__64-brightgreen)
![Itsuki](https://img.shields.io/badge/itsuki-5.5.0%2B-9cf)
![Licencia](https://img.shields.io/badge/licencia-MIT-green)
[![Discord](https://img.shields.io/badge/Discord-Unirse-7289DA?logo=discord&logoColor=white)](https://discord.gg/K6SEpB7Pf5)

---

## Descripción

**sukets** es una librería de sockets para **itsuki-lang** que abstrae las diferencias entre Winsock2 (Windows) y POSIX sockets (Linux) bajo una API única. Provee TCP cliente, TCP servidor, UDP, sockets raw, resolución DNS, multiplexación (`select`), timeouts y envío/recepción completos, con códigos de error unificados entre plataformas.

> **Estado:** fase experimental. La API puede cambiar sin previo aviso.
> **Compatibilidad:** Itsuki 5.5.0 o superior, arquitecturas x86_32 y x86_64.

---

## Características

- **Multiplataforma:** mismo código fuente para Windows y Linux.
- **TCP cliente** con resolución DNS (IP o nombre de dominio).
- **TCP servidor** con backlog configurable y `SO_REUSEADDR`.
- **UDP completo:** open, bind, sendto, recvfrom, broadcast, close.
- **Sockets raw:** ICMP/TCP/UDP crudo, con `IP_HDRINCL` en Linux.
- **Multiplexación:** `sk_select()` atiende varios sockets sin threads.
- **Envío y recepción completos** (`sk_send_all` / `sk_recv_all`).
- **Opciones de socket:** timeout, `TCP_NODELAY`, keepalive, reuseaddr, `sk_shutdown()`, IP del peer, puerto local.
- **Errores unificados:** `sk_last_error()` devuelve los mismos códigos en Windows y Linux.
- **Ciclo de vida explícito:** `sk_init()` / `sk_cleanup()`.

---

## Instalación

### Vía gestor de paquetes de Itsuki (recomendado)

~~~bash
itsuki install mateocollar/sukets
~~~

El paquete queda disponible globalmente en `~/.itsuki/packages/`.

### Manual

Copiá `sukets.huki` y `sukets.c` en tu proyecto. En Windows con MinGW, asegurate de enlazar `ws2_32`.

---

## Uso básico

### TCP — Cliente

~~~itsuki
#include <fundamental.huki>
#include <sukets.huki>

fn main() -> int {
    sk_init();

    var sock: int = sk_tcp_connect("example.com", 80);
    if (sock < 0) {
        // -1 init, -2 socket, -3 DNS, -4 connect
        return 1;
    }

    sk_send_all(sock, "Hola", 4);
    sk_shutdown(sock, 1);   // terminé de enviar; sigo recibiendo
    sk_close(sock);

    sk_cleanup();
    return 0;
}
~~~

### TCP — Servidor (multiplexado con select)

~~~itsuki
#include <fundamental.huki>
#include <sukets.huki>

fn main() -> int {
    sk_init();

    var srv: int = sk_tcp_server(0, 8080, 16);
    if (srv < 0) {
        return 1;
    }

    var clients: int[16];
    var count: int = 1;
    clients[0] = srv;

    while (true) {
        var idx: int = sk_select(clients, count, 1000);
        if (idx == -1) { break; }
        if (idx == -2) { continue; }   // timeout: seguir esperando

        if (clients[idx] == srv) {
            var client: int = sk_accept(srv);
            if (client >= 0 && count < 16) {
                clients[count] = client;
                count++;
            }
        } else {
            var buf: char[1024];
            var n: int = sk_recv(clients[idx], buf, 1024);
            if (n <= 0) {
                sk_close(clients[idx]);
                clients[idx] = clients[count - 1];
                count--;
            } else {
                sk_send_all(clients[idx], buf, n);   // eco
            }
        }
    }

    var i: int = 0;
    while (i < count) {
        sk_close(clients[i]);
        i++;
    }
    sk_cleanup();
    return 0;
}
~~~

### UDP

~~~itsuki
#include <fundamental.huki>
#include <sukets.huki>

fn main() -> int {
    var sock: int = sk_udp_open();
    sk_udp_bind(sock, 0, 9000);

    var buf: char[512];
    var ip: char[46];
    var port: int;
    sk_udp_recvfrom(sock, buf, 512, ip, &port);

    sk_udp_close(sock);
    return 0;
}
~~~

### Compilación con Itsuki

~~~bash
# Ejecutar directamente (NO pases sukets.c: Itsuki lo encuentra solo)
itsuki run main.suki -lws2_32   # Windows (ws2_32)
itsuki run main.suki            # Linux (sockets vienen en libc)

# Generar binario
itsuki build main.suki -o mi_programa

# Verificar el entorno antes de compilar
itsuki doctor
~~~

> ⚠️ Si pasás `sukets.c` explícitamente en la línea de comandos, Itsuki lo
> compila dos veces (explícito + autodescubrimiento) y el linker falla con
> `multiple definition`.

---

## API

### Ciclo de vida

| Función | Descripción |
|---|---|
| `sk_init()` | Inicializa la librería. Llamar una vez al inicio (idempotente). |
| `sk_cleanup()` | Libera recursos. Llamar al finalizar. |
| `sk_last_error()` | Último error como código `SkError` unificado (portable). |
| `sk_native_error()` | Último error crudo del SO (debugging). |

### TCP

| Función | Descripción |
|---|---|
| `sk_tcp_connect(host, port)` | Conecta a host:port (IP o DNS). Devuelve socket o error (-1 a -4). |
| `sk_tcp_server(host, port, backlog)` | Crea socket en escucha. `host = 0` para todas las interfaces. |
| `sk_accept(server_sock)` | Acepta una conexión entrante. |
| `sk_shutdown(sock, how)` | Cierra un sentido sin liberar el descriptor. `how`: 0 recv, 1 send, 2 ambos. |

### Envío y recepción

| Función | Descripción |
|---|---|
| `sk_send(sock, data, len)` | Envía (semántica clásica). |
| `sk_recv(sock, buf, max_len)` | Recibe (semántica clásica). |
| `sk_send_all(sock, data, len)` | Garantiza el envío completo. |
| `sk_recv_all(sock, buf, max_len)` | Garantiza la recepción completa; agrega `'\0'`. |

### Multiplexación

| Función | Descripción |
|---|---|
| `sk_select(socks, count, timeout_ms)` | Espera a que cualquier socket tenga datos. Devuelve el índice listo, -2 en timeout, -1 en error. |

### Opciones

| Función | Descripción |
|---|---|
| `sk_set_timeout(sock, ms)` | Timeout de recv/send en milisegundos (0 = bloqueante). |
| `sk_set_nodelay(sock, enabled)` | Activa/desactiva TCP_NODELAY. |
| `sk_set_keepalive(sock, enabled)` | Detección de peers muertos a nivel TCP. |
| `sk_set_reuseaddr(sock, enabled)` | Reutiliza la dirección local (TIME_WAIT). |
| `sk_peer_ip(sock, ip_buf, ip_len)` | Obtiene la IP del otro extremo. |
| `sk_local_port(sock)` | Puerto local del socket (útil tras bind con puerto 0). |
| `sk_close(sock)` | Cierra el socket (TCP y UDP). |

### UDP

| Función | Descripción |
|---|---|
| `sk_udp_open()` | Crea un socket UDP no conectado. |
| `sk_udp_bind(sock, host, port)` | Liga a un puerto local (0 = efímero). |
| `sk_udp_sendto(sock, host, port, data, len)` | Envía un datagrama. |
| `sk_udp_recvfrom(sock, buf, max_len, src_ip, src_port)` | Recibe un datagrama; escribe IP/puerto del remitente. |
| `sk_udp_broadcast(sock, enabled)` | Habilita envío broadcast. |
| `sk_udp_close(sock)` | Cierra el socket UDP. |

### Raw (requiere privilegios)

| Función | Descripción |
|---|---|
| `sk_raw_open(protocol)` | Crea un socket raw. `protocol`: 1=ICMP, 6=TCP, 17=UDP. |
| `sk_raw_include_header(sock, enabled)` | Vos construís la cabecera IP (IP_HDRINCL). Solo Linux. |
| `sk_raw_sendto(sock, host, protocol, data, len)` | Envía un paquete raw. |
| `sk_raw_recvfrom(sock, buf, max_len, src_ip)` | Recibe un paquete raw. |
| `sk_raw_close(sock)` | Cierra el socket raw. |

---

## Códigos de error

### Por función (retorno negativo)

- **sk_tcp_connect:** `-1` init, `-2` socket, `-3` DNS, `-4` connect.
- **sk_tcp_server:** `-1` init, `-2` socket, `-3` bind, `-4` listen.
- **sk_raw_include_header:** `-9` en Windows (no soportado).

### `sk_last_error()` — códigos unificados (`enum SkError`)

| Código | Constante | Significado |
|---|---|---|
| 0 | `SK_E_OK` | Sin error. |
| 1 | `SK_E_WOULD_BLOCK` | No hay datos todavía. |
| 2 | `SK_E_CONN_RESET` | El peer cerró abruptamente. |
| 3 | `SK_E_CONN_REFUSED` | Nadie escucha en ese puerto. |
| 4 | `SK_E_TIMED_OUT` | Venció el timeout. |
| 5 | `SK_E_UNREACH` | Red inalcanzable. |
| 6 | `SK_E_IN_PROGRESS` | Conexión en curso. |
| 8 | `SK_E_ACCESS` | Falta de permisos (raw, puerto < 1024). |
| -1 | — | Desconocido: consultar `sk_native_error()`. |

### Migración desde v2.0

`sukets_connect` → `sk_tcp_connect`, `sukets_server_create` → `sk_tcp_server`, `sukets_get_last_error` → `sk_last_error`/`sk_native_error`, y el resto `sukets_*` → `sk_*`. Tabla completa en el [CHANGELOG.md](CHANGELOG.md).

---

# Mantenedor

Esta librería está hecha y mantenida oficialmente por [Mateo Collar](https://github.com/mateocollar/sukets).

---

## Changelog

Ver [CHANGELOG.md](CHANGELOG.md).

---

## Contribuir

- 🐛 [Reportar bugs](https://github.com/mateocollar/sukets/issues)
- 🛠️ [Enviar Pull Requests](https://github.com/mateocollar/sukets/pulls)
- 💬 [Discord de Itsuki](https://discord.gg/K6SEpB7Pf5)

---

## Licencia

MIT. Ver [LICENSE](LICENSE).

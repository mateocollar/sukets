# sukets

Librería de sockets simple y multiplataforma para [itsuki-lang](https://github.com/Apolo1061/itsuki-lang).

![Versión](https://img.shields.io/badge/versi%C3%B3n-2.0.0-blue)
![Estado](https://img.shields.io/badge/estado-experimental-orange)
![Plataformas](https://img.shields.io/badge/plataformas-Windows%20%7C%20Linux-lightgrey)
![Arquitectura](https://img.shields.io/badge/arquitectura-x86__32%20%7C%20x86__64-brightgreen)
![Itsuki](https://img.shields.io/badge/itsuki-5.5.0%2B-9cf)
![Licencia](https://img.shields.io/badge/licencia-MIT-green)
[![Discord](https://img.shields.io/badge/Discord-Unirse-7289DA?logo=discord&logoColor=white)](https://discord.gg/K6SEpB7Pf5)

---

## Descripción

**sukets** es una librería de sockets para **itsuki-lang** que abstrae las diferencias entre Winsock2 (Windows) y POSIX sockets (Linux) bajo una API única. Provee TCP cliente, TCP servidor, UDP, resolución DNS, timeouts y envío/recepción completos.

> **Estado:** fase experimental. La API puede cambiar sin previo aviso.
> **Compatibilidad:** Itsuki 5.5.0 o superior, arquitecturas x86_32 y x86_64.

---

## Características

- **Multiplataforma:** mismo código fuente para Windows y Linux.
- **TCP cliente** con resolución DNS (IP o nombre de dominio).
- **TCP servidor** con backlog configurable y `SO_REUSEADDR`.
- **UDP completo:** open, bind, sendto, recvfrom, broadcast, close.
- **Envío y recepción completos** (`send_all` / `recv_all`).
- **Opciones de socket:** timeout, `TCP_NODELAY`, IP del peer.
- **Ciclo de vida explícito:** `sukets_init()` / `sukets_cleanup()`.

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

~~~c
#include <fundamental.huki>
#include <sukets.huki>

fn main() -> int {
    sukets_init();

    var sock: int = sukets_connect("example.com", 80);
    if (sock < 0) {
        // -1 init, -2 socket, -3 DNS, -4 connect
        return 1;
    }

    sukets_send_all(sock, "Hola", 4);
    sukets_close(sock);

    sukets_cleanup();
    return 0;
}
~~~

### TCP — Servidor

~~~c
#include <fundamental.huki>
#include <sukets.huki>

fn main() -> int {
    sukets_init();

    var srv: int = sukets_server_create(0, 8080, 16);
    if (srv < 0) {
        return 1;
    }

    var client: int = sukets_accept(srv);
    var buf: char[1024];
    sukets_recv_all(client, buf, 1024);

    sukets_close(client);
    sukets_close(srv);
    sukets_cleanup();
    return 0;
}
~~~

### UDP

~~~c
#include <fundamental.huki>
#include <sukets.huki>

fn main() -> int {
    var sock: int = sukets_udp_open();
    sukets_udp_bind(sock, 0, 9000);

    var buf: char[512];
    var ip: char[46];
    var port: int;
    sukets_udp_recvfrom(sock, buf, 512, ip, &port);

    sukets_udp_close(sock);
    return 0;
}
~~~

### Compilación con Itsuki

~~~bash
# Ejecutar directamente (incluyendo el runtime C de sukets)
itsuki run main.suki sukets.c

# Generar binario
itsuki build main.suki sukets.c -o mi_programa
~~~

Verificá el entorno antes de compilar:

~~~bash
itsuki doctor
~~~

---

## API

### Ciclo de vida

| Función | Descripción |
|---|---|
| `sukets_init()` | Inicializa la librería. Llamar una vez al inicio. |
| `sukets_cleanup()` | Libera recursos. Llamar al finalizar. |
| `sukets_get_last_error()` | Devuelve el último código de error del SO. |

### TCP — Cliente

| Función | Descripción |
|---|---|
| `sukets_connect(host, port)` | Conecta a host:port. Devuelve socket o error (-1 a -4). |

### TCP — Servidor

| Función | Descripción |
|---|---|
| `sukets_server_create(host, port, backlog)` | Crea socket en escucha. `host = 0` para todas las interfaces. |
| `sukets_accept(server_sock)` | Acepta una conexión entrante. |

### Envío y recepción

| Función | Descripción |
|---|---|
| `sukets_send(sock, data, len)` | Envía (semántica clásica). |
| `sukets_recv(sock, buf, max_len)` | Recibe (semántica clásica). |
| `sukets_send_all(sock, data, len)` | Garantiza el envío completo. |
| `sukets_recv_all(sock, buf, max_len)` | Garantiza la recepción completa; agrega `'\0'`. |

### Opciones

| Función | Descripción |
|---|---|
| `sukets_set_timeout(sock, ms)` | Timeout de recv/send en milisegundos (0 = bloqueante). |
| `sukets_set_nodelay(sock, enabled)` | Activa/desactiva TCP_NODELAY. |
| `sukets_peer_ip(sock, ip_buf, ip_len)` | Obtiene la IP del otro extremo. |
| `sukets_close(sock)` | Cierra el socket. |

### UDP

| Función | Descripción |
|---|---|
| `sukets_udp_open()` | Crea un socket UDP no conectado. |
| `sukets_udp_bind(sock, host, port)` | Liga a un puerto local. |
| `sukets_udp_sendto(sock, host, port, data, len)` | Envía un datagrama. |
| `sukets_udp_recvfrom(sock, buf, max_len, src_ip, src_port)` | Recibe un datagrama; escribe IP/puerto del remitente. |
| `sukets_udp_broadcast(sock, enabled)` | Habilita envío broadcast. |
| `sukets_udp_close(sock)` | Cierra el socket UDP. |

---

## Códigos de error

**sukets_connect:** `-1` init, `-2` socket, `-3` DNS, `-4` connect.
**sukets_server_create:** `-1` init, `-2` socket, `-3` resolve/bind, `-4` listen.

---

# Mantenedor

Esta librería está hecha y mantenida oficialmente por [Mateo Collar](https://github.com/mateocollar/sukets)

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
# Changelog

## [1.0.0] - 2026-10-17
### Añadido
- Soporte inicial para Sockets TCP (Cliente).
- Funciones para conectar (`sukets_connect`), enviar (`sukets_send`), recibir (`sukets_recv`) y cerrar (`sukets_close`).

## [1.1.0] - 2026-10-17
### Añadido
- **Manejo avanzado de errores**: Nuevos códigos de error descriptivos para fallos de Winsock, IPs inválidas y tiempos de espera.
- **Nuevas funciones de red**:
  - `sukets_get_last_error()`: Recupera el último código de error del sistema operativo.
  - `sukets_set_timeout()`: Configura un tiempo de límite (timeout) para las operaciones de lectura/escritura.
- **Optimización en recepción**: Mejor gestión del búfer en `sukets_recv()` para evitar desbordamientos.

## [2.0.0] - 2026-10-20
### Añadido
- **Compatibilidad multiplataforma**: mismo código fuente compila en Windows (Winsock2) y Linux (POSIX sockets).
- **Ciclo de vida explícito**: `sukets_init()` y `sukets_cleanup()` reemplazan el arranque/limpieza automática por conexión.
- **Resolución DNS**: `sukets_connect()` acepta nombres de dominio además de IPs (`getaddrinfo`).
- **Servidor TCP**: `sukets_server_create()` y `sukets_accept()`, con `SO_REUSEADDR`.
- **UDP completo**: `sukets_udp_open/bind/sendto/recvfrom/broadcast/close`, con IP y puerto del remitente en recepción.
- **Envío y recepción completos**: `sukets_send_all()` y `sukets_recv_all()` garantizan procesar todos los bytes.
- **Opciones**: `sukets_set_nodelay()` (TCP_NODELAY) y `sukets_peer_ip()`.

### Corregido
- `sukets_recv()` ya no reserva un byte para `'\0'` (ese comportamiento pasó a `sukets_recv_all()`).
- `sukets_close()` ya no ejecuta `WSACleanup()`, lo que invalidaba otros sockets abiertos.

### Cambiado
- `sukets_connect()` devuelve códigos de error más granulares (-1 a -4).

# Changelog

## [2.2.0] - 2026-10-22
### Añadido
- **Sockets raw**: `sk_raw_open()`, `sk_raw_sendto()`, `sk_raw_recvfrom()` y `sk_raw_include_header()` (IP_HDRINCL, solo Linux; en Windows devuelve -9). Requieren privilegios: root/CAP_NET_RAW en Linux, admin en Windows.
- **Multiplexación**: `sk_select(socks, count, timeout_ms)` permite atender varios sockets sin threads. Devuelve el índice del socket listo, -2 en timeout o -1 en error.
- **Errores unificados**: `sk_last_error()` devuelve los mismos códigos en Windows y Linux (enum `SkError` en el header). `sk_native_error()` expone el código crudo del SO para debugging.
- **Nuevas opciones**: `sk_shutdown(sock, how)` (cierre ordenado de un sentido), `sk_set_keepalive()`, `sk_set_reuseaddr()`, `sk_local_port()`.
- **Documentación completa**: cada función de `sukets.huki` documenta comportamiento, parámetros, retornos y códigos de error.

### Cambiado
- **API renombrada (breaking)**: prefijo `sukets_` → `sk_`. Se mantiene un prefijo corto porque `send`, `recv`, `connect` y `close` colisionan con la libc.
- Nombres más descriptivos: `sk_tcp_connect()` / `sk_tcp_server()` en vez de un genérico `sk_connect()`.
- `sukets_get_last_error()` se divide en `sk_last_error()` (unificado) y `sk_native_error()` (crudo).

### Migración desde 2.0.0
| v2.0 | v2.2 |
|---|---|
| `sukets_init` | `sk_init` |
| `sukets_cleanup` | `sk_cleanup` |
| `sukets_connect` | `sk_tcp_connect` |
| `sukets_server_create` | `sk_tcp_server` |
| `sukets_get_last_error` | `sk_last_error` (unificado) / `sk_native_error` (crudo) |
| `sukets_*` (resto) | `sk_*` |

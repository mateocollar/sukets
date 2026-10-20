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
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
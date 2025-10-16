Librerías utilizadas
Este proyecto usa las siguientes cabeceras de C para implementar el broker, el publicador (publisher) y el suscriptor (subscriber) en Windows.

<winsock2.h>
API de sockets de Windows (Winsock). Permite crear y usar conexiones TCP/UDP.
Usos en el proyecto:
- Inicialización y finalización de Winsock: WSAStartup(), WSACleanup().
- Sockets TCP: socket(), bind(), listen(), accept(), connect(), send(), recv(), closesocket().
- Multiplexación sin hilos: select(), FD_ZERO, FD_SET, FD_ISSET.
- Tipos/constantes: SOCKET, INVALID_SOCKET, SOCKET_ERROR.

Notas:
- Requiere enlazar con la librería del sistema: ws2_32.lib (MSVC) o -lws2_32 (MinGW).

<ws2tcpip.h>

Extensiones de Winsock para TCP/IP y utilidades de direcciones.
Usos en el proyecto:
- Conversión de IP en texto a binario: inet_pton() (y opcionalmente inet_ntop()).
- Estructuras/constantes adicionales para IPv4/IPv6.

Notas:
- Incluir después de <winsock2.h>.

<stdio.h>
Entrada/salida estándar y formateo de cadenas.
Usos en el proyecto:

- Mensajes y logs: printf(), fprintf(), fputs().
- Construcción de líneas del protocolo: snprintf() / _snprintf() (MSVC).

<stdlib.h>
Utilidades generales de la biblioteca estándar.
Usos en el proyecto:

- Conversión de cadenas a entero para puertos: atoi().

<string.h>
Operaciones sobre cadenas y memoria.
Usos en el proyecto:

- Copia/limpieza de buffers: memset(), memcpy(), memmove(), strncpy().
- Comparación/búsqueda: strcmp(), strncmp(), strchr(), strcspn().

Compilación del proyecto

Para el caso de TCP es necesario acceder a la carpeta del proyecto desde la terminal de x64 Native Tool Command Prompt for Visual Insider y correr los comandos:
Para la creación del objeto:
- cl broker_tcp.c ws2_32.lib
- cl publisher_tcp.c ws2_32.lib
- cl subscriber_tcp.c ws2_32.lib

Para la ejecución:
- broker_tcp
- subscriber_tcp ó subscriber_tcp.exe 127.0.0.1 5000 PARTIDO_A PARTIDO_B
- publisher_tcp ó publisher_tcp.exe 127.0.0.1 5000 PARTIDO_B

/* ===== Solo Windows (mínimo) ===== */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

static int  net_init(void){ WSADATA d; return WSAStartup(MAKEWORD(2,2), &d); }
static void net_cleanup(void){ WSACleanup(); }
static void net_close(SOCKET s){ closesocket(s); }

/* ===== Utilidades mínimas ===== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 5000
#define MAX_LINE    1024

static int build_sub(char *out, size_t n, const char *topic){
    return _snprintf(out, n, "SUB %s\n", topic);  /* _snprintf en MSVC */
}

/* --- subscriber_tcp.c --- */
int main(int argc, char** argv){
    if (net_init() != 0){ fputs("Winsock init failed\n", stderr); return 1; }

    const char* host  = (argc > 1) ? argv[1] : "127.0.0.1";
    int         port  = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET){ fputs("socket() failed\n", stderr); net_cleanup(); return 1; }

    struct sockaddr_in srv; memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port   = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &srv.sin_addr) != 1){
        fputs("inet_pton() failed\n", stderr); net_close(s); net_cleanup(); return 1;
    }
    if (connect(s, (struct sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR){
        fputs("connect() failed\n", stderr); net_close(s); net_cleanup(); return 1;
    }

    /* Enviar SUB(s): si no hay topics por CLI, usa PARTIDO_A */
    if (argc <= 3){
        char line[MAX_LINE];
        int  n = build_sub(line, sizeof(line), "PARTIDO_A");
        if (n > 0) (void)send(s, line, n, 0);
        printf("Suscrito a PARTIDO_A. Esperando mensajes...\n");
    } else {
        printf("Suscrito a: ");
        for (int i = 3; i < argc; ++i){
            char line[MAX_LINE];
            int  n = build_sub(line, sizeof(line), argv[i]);
            if (n > 0) (void)send(s, line, n, 0);
            printf("%s%s", argv[i], (i+1<argc)? ", ":"");
        }
        printf(". Esperando mensajes...\n");
    }

    /* Recibir e imprimir */
    char buf[2048];
    for (;;){
        int n = recv(s, buf, (int)sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        fputs(buf, stdout);
    }

    net_close(s);
    net_cleanup();
    return 0;
}

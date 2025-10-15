// ===== Imports =====
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  net_init(void){ WSADATA d; return WSAStartup(MAKEWORD(2,2), &d); }
static void net_cleanup(void){ WSACleanup(); }
static void net_close(SOCKET s){ closesocket(s); }


// ===== Configurar identificadores =====
#define DEFAULT_PORT 5000
#define MAX_LINE 1024
#define MAX_TOPIC 64
#define MAX_MSG 900
#define HAS_PREFIX(s,lit) (strncmp((s),(lit),sizeof(lit)-1)==0)
#define SKIP_SPACES(p) while (*(p)==' ') ++(p)

// ===== Crear Publicador =====
static int build_pub(char *out, size_t n, const char *topic, const char *msg){
    return snprintf(out, n, "PUB %s %s\n", topic, msg);
}

// ===== Parsear línea de PUB =====
static int parse_pub(const char *line, char *topic, size_t tsize, char *msg, size_t msize){
    if (!HAS_PREFIX(line, "PUB ")) return 0;
    const char *p = line + 4; SKIP_SPACES(p);
    size_t tn = strcspn(p, " \r\n");
    if (tn == 0 || tn >= tsize) return -1;
    memcpy(topic, p, tn); topic[tn] = '\0';
    const char *m = p + tn; SKIP_SPACES(m);
    size_t mn = strcspn(m, "\r\n");
    if (mn >= msize) return -1;
    memcpy(msg, m, mn); msg[mn] = '\0';
    return 1;
}

// ===== Main =====
int main(int argc, char** argv){
    if (net_init() != 0){ fputs("Winsock init fallo\n", stderr); return 1; }

    // Formato del comando
    const char* host  = (argc > 1) ? argv[1] : "127.0.0.1";
    int         port  = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    const char* topic = (argc > 3) ? argv[3] : "PARTIDO_A";

    // Creación del socket y conexión
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET){ fputs("socket() fallo\n", stderr); net_cleanup(); return 1; }

    struct sockaddr_in srv; memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port   = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &srv.sin_addr) != 1){
        fputs("inet_pton() fallo\n", stderr); net_close(s); net_cleanup(); return 1;
    }

    if (connect(s, (struct sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR){
        fputs("connect() fallo\n", stderr); net_close(s); net_cleanup(); return 1;
    }

    // Enviar 10 mensajes de gol 
    for (int i = 1; i <= 10; ++i){
        char body[MAX_MSG];
        char line[MAX_LINE];

        int blen = snprintf(body, sizeof(body), "Gol %d del %s!", i, topic);
        if (blen <= 0 || blen >= (int)sizeof(body)) continue;

        int llen = build_pub(line, sizeof(line), topic, body);
        if (llen > 0) (void)send(s, line, llen, 0);
    }

    net_close(s);
    net_cleanup();
    return 0;
}

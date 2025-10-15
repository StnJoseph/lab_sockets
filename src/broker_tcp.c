/* ===== Solo Windows (mínimo) ===== */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

static int  net_init(void){ WSADATA d; return WSAStartup(MAKEWORD(2,2), &d); }
static void net_cleanup(void){ WSACleanup(); }
static void net_close(SOCKET s){ closesocket(s); }

/* ===== Protocolo mínimo ===== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 5000
#define MAX_LINE    1024
#define MAX_TOPIC     64
#define MAX_MSG      900

#define HAS_PREFIX(s,lit) (strncmp((s),(lit),sizeof(lit)-1)==0)
#define SKIP_SPACES(p)    while (*(p)==' ') ++(p)

/* Construcción */
static int build_sub(char *out, size_t n, const char *topic){
    return snprintf(out, n, "SUB %s\n", topic);
}
static int build_pub(char *out, size_t n, const char *topic, const char *msg){
    return snprintf(out, n, "PUB %s %s\n", topic, msg);
}

/* Parseo: 1=ok, 0=no coincide, -1=mal formato */
static int parse_sub(const char *line, char *topic, size_t tsize){
    if (!HAS_PREFIX(line, "SUB ")) return 0;
    const char *p = line + 4; SKIP_SPACES(p);
    size_t n = strcspn(p, "\r\n");
    if (n == 0 || n >= tsize) return -1;
    memcpy(topic, p, n); topic[n] = '\0';
    return 1;
}
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

/* Puerto por argv o por defecto */
static int get_port(int argc, char **argv, int fallback){
    return (argc > 1) ? atoi(argv[1]) : fallback;
}



// --- broker_tcp.c (solo TCP) ---
// --- broker_tcp.c (Windows-only, simple) ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIENTS 64

typedef struct {
    SOCKET sock;
    int    active;
    char   topic[MAX_TOPIC];   /* "" = sin suscripción */
    char   buf[MAX_LINE];      /* ensamblado de líneas */
    size_t len;
} Client;

static void client_close(Client* c){
    if (c->active) net_close(c->sock);
    c->sock = INVALID_SOCKET; c->active = 0;
    c->topic[0] = c->buf[0] = 0; c->len = 0;
}

static int send_all(SOCKET s, const char* data, int len){
    int sent = 0;
    while (sent < len){
        int n = send(s, data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

static void broadcast_topic(Client* cs, int n, const char* topic, const char* body){
    char out[MAX_LINE];
    int m = snprintf(out, sizeof(out), "MSG %s %s\n", topic, body);
    for (int i = 0; i < n; ++i){
        if (!cs[i].active) continue;
        if (cs[i].topic[0] == 0) continue;
        if (strcmp(cs[i].topic, topic) == 0) send_all(cs[i].sock, out, m);
    }
}

int main(int argc, char** argv){
    if (net_init() != 0){ fprintf(stderr, "Init Winsock falló\n"); return 1; }
    int port = get_port(argc, argv, DEFAULT_PORT);

    SOCKET lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock == INVALID_SOCKET){ fprintf(stderr, "socket() falló\n"); net_cleanup(); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(lsock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR){
        fprintf(stderr, "bind() falló (puerto en uso?)\n");
        net_close(lsock); net_cleanup(); return 1;
    }
    if (listen(lsock, 16) == SOCKET_ERROR){
        fprintf(stderr, "listen() falló\n");
        net_close(lsock); net_cleanup(); return 1;
    }

    printf("Broker TCP escuchando en puerto %d...\n", port);

    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i){ clients[i].active = 0; clients[i].sock = INVALID_SOCKET; clients[i].topic[0] = clients[i].buf[0] = 0; clients[i].len = 0; }

    fd_set readfds;

    for (;;){
        FD_ZERO(&readfds);
        FD_SET(lsock, &readfds);
        for (int i = 0; i < MAX_CLIENTS; ++i) if (clients[i].active) FD_SET(clients[i].sock, &readfds);

        /* En Winsock, el primer parámetro de select() se ignora; pasa 0. */
        int ready = select(0, &readfds, NULL, NULL, NULL);
        if (ready <= 0) continue;

        /* Nueva conexión */
        if (FD_ISSET(lsock, &readfds)){
            struct sockaddr_in cli; socklen_t clen = sizeof(cli);
            SOCKET cs = accept(lsock, (struct sockaddr*)&cli, &clen);
            if (cs != INVALID_SOCKET){
                int placed = 0;
                for (int i = 0; i < MAX_CLIENTS; ++i){
                    if (!clients[i].active){
                        clients[i].active = 1; clients[i].sock = cs;
                        clients[i].topic[0] = clients[i].buf[0] = 0; clients[i].len = 0;
                        placed = 1; break;
                    }
                }
                if (!placed){
                    const char* fullmsg = "MSG SYS Servidor lleno\n";
                    send_all(cs, fullmsg, (int)strlen(fullmsg));
                    net_close(cs);
                }
            }
        }

        /* Datos de clientes */
        for (int i = 0; i < MAX_CLIENTS; ++i){
            if (!clients[i].active) continue;
            Client* c = &clients[i];
            if (!FD_ISSET(c->sock, &readfds)) continue;

            int n = recv(c->sock, c->buf + c->len, (int)(sizeof(c->buf) - c->len - 1), 0);
            if (n <= 0){ client_close(c); continue; }

            c->len += n; c->buf[c->len] = '\0';

            char* start = c->buf;
            for (;;){
                char* nl = strchr(start, '\n');
                if (!nl) break;
                *nl = '\0';

                char topic[MAX_TOPIC], body[MAX_MSG];
                int rsub = parse_sub(start, topic, sizeof(topic));
                if (rsub == 1){
                    strncpy(c->topic, topic, sizeof(c->topic) - 1);
                    c->topic[sizeof(c->topic) - 1] = '\0';
                } else if (rsub == 0){
                    int rpub = parse_pub(start, topic, sizeof(topic), body, sizeof(body));
                    if (rpub == 1) broadcast_topic(clients, MAX_CLIENTS, topic, body);
                }
                start = nl + 1;
            }

            /* Compactar buffer */
            size_t rem = (size_t)(c->buf + c->len - start);
            memmove(c->buf, start, rem);
            c->len = rem; c->buf[c->len] = '\0';
        }
    }

    /* (no se alcanza normalmente) */
    net_close(lsock);
    net_cleanup();
    return 0;
}

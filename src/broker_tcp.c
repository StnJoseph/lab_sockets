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
#define MAX_CLIENTS 64
#define MAX_LINE 1024
#define MAX_TOPIC 64
#define MAX_SUBS 8

typedef struct {
    SOCKET sock; 
    int    active;
    char   topics[MAX_SUBS][MAX_TOPIC];
    int    nsubs;
    char   buf[MAX_LINE];
    size_t len;
} Client;

// ==== Funcion enviar el buffer ====
static int send_all(SOCKET s, const char* data, int len){
    int sent = 0;
    while (sent < len){
        int n = send(s, data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

// ==== Cerrar cliente ====
static void client_close(Client* c){
    if (c->active) net_close(c->sock);
    memset(c, 0, sizeof(*c));
    c->sock = INVALID_SOCKET;
}

// ==== Verificar subscripcion del cliente ====
static int has_sub(const Client* c, const char* topic){
    for (int i = 0; i < c->nsubs; ++i)
        if (strcmp(c->topics[i], topic) == 0) return 1;
    return 0;
}

// ==== Agregar subscripcion del cliente ====
static void add_sub(Client* c, const char* topic){
    if (c->nsubs >= MAX_SUBS || has_sub(c, topic)) return;
    strncpy(c->topics[c->nsubs], topic, MAX_TOPIC-1);
    c->topics[c->nsubs][MAX_TOPIC-1] = '\0';
    c->nsubs++;
}

// ==== Envia el mensaje a todos los subscriptores segun el topic ====
static void broadcast_topic(Client* cs, int n, const char* topic, const char* body){
    char out[MAX_LINE];
    int m = snprintf(out, sizeof(out), "MSG %s %s\n", topic, body);
    if (m <= 0) return;
    if (m > (int)sizeof(out)) m = (int)sizeof(out); /* clamp por seguridad */
    for (int i = 0; i < n; ++i)
        if (cs[i].active && has_sub(&cs[i], topic)) (void)send_all(cs[i].sock, out, m);
}

// ==== Parser SUB y PUB ====
typedef enum { CMD_NONE=0, CMD_SUB, CMD_PUB, CMD_BAD } Cmd;

static Cmd parse_line(char *line, char **out_topic, char **out_body){
    while (*line==' ') ++line;

    if (strncmp(line,"SUB ",4)==0){
        char *p = line+4; while (*p==' ') ++p;
        if (*p=='\0') return CMD_BAD;
        char *end = p; while (*end && *end!='\r' && *end!='\n') ++end;
        *out_topic = p; *end = '\0'; *out_body = NULL;
        return CMD_SUB;
    }

    if (strncmp(line,"PUB ",4)==0){
        char *p = line+4; while (*p==' ') ++p;
        if (*p=='\0') return CMD_BAD;
        char *t = p; while (*p && *p!=' ' && *p!='\r' && *p!='\n') ++p;
        if (*p=='\0' || *p=='\r' || *p=='\n') return CMD_BAD;
        *p++ = '\0'; while (*p==' ') ++p;
        char *b = p; char *end = b; while (*end && *end!='\r' && *end!='\n') ++end;
        *out_topic = t; *out_body = b; *end = '\0';
        return CMD_PUB;
    }

    return CMD_NONE;
}

// ==== Main ====
static int get_port(int argc, char **argv, int fallback){ return (argc>1)? atoi(argv[1]) : fallback; }

int main(int argc, char** argv){
    if (net_init() != 0){ fputs("Winsock init fallo\n", stderr); return 1; }
    int port = get_port(argc, argv, DEFAULT_PORT);

    // Creación del socket y conexión
    SOCKET lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock == INVALID_SOCKET){ fputs("socket() fallo\n", stderr); net_cleanup(); return 1; }

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(lsock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR){
        fputs("bind() fallo\n", stderr); net_close(lsock); net_cleanup(); return 1;
    }
    if (listen(lsock, 16) == SOCKET_ERROR){
        fputs("listen() fallo\n", stderr); net_close(lsock); net_cleanup(); return 1;
    }

    printf("Broker TCP escuchando en puerto %d...\n", port);

    Client clients[MAX_CLIENTS];
    for (int i=0;i<MAX_CLIENTS;++i){ memset(&clients[i], 0, sizeof(Client)); clients[i].sock = INVALID_SOCKET; }

    fd_set readfds;
    for (;;){
        FD_ZERO(&readfds);
        FD_SET(lsock, &readfds);
        for (int i=0;i<MAX_CLIENTS;++i) if (clients[i].active) FD_SET(clients[i].sock, &readfds);

        if (select(0, &readfds, NULL, NULL, NULL) <= 0) continue;

        // Aceptar nuevas conexiones
        if (FD_ISSET(lsock, &readfds)){
            struct sockaddr_in cli; int clen = sizeof(cli);
            SOCKET cs = accept(lsock, (struct sockaddr*)&cli, &clen);
            if (cs != INVALID_SOCKET){
                int placed = 0;
                for (int i=0;i<MAX_CLIENTS;++i){
                    if (!clients[i].active){
                        memset(&clients[i], 0, sizeof(Client));
                        clients[i].active = 1; clients[i].sock = cs;
                        placed = 1; break;
                    }
                }
                if (!placed){
                    static const char fullmsg[] = "MSG SYS Servidor lleno\n";
                    (void)send_all(cs, fullmsg, (int)sizeof(fullmsg)-1);
                    net_close(cs);
                }
            }
        }

        // Leer datos de clientes
        for (int i=0;i<MAX_CLIENTS;++i){
            if (!clients[i].active) continue;
            Client *c = &clients[i];
            if (!FD_ISSET(c->sock, &readfds)) continue;

            int n = recv(c->sock, c->buf + (int)c->len, (int)(sizeof(c->buf) - c->len - 1), 0);
            if (n <= 0){ client_close(c); continue; }

            c->len += n; c->buf[c->len] = '\0';

            // Procesar líneas completas
            char *start = c->buf;
            for (;;){
                char *nl = strchr(start, '\n');
                if (!nl) break;
                *nl = '\0';

                char *topic=NULL, *body=NULL;
                switch (parse_line(start, &topic, &body)){
                    case CMD_SUB: if (topic && *topic) add_sub(c, topic); break;
                    case CMD_PUB: if (topic && body && *topic && *body) broadcast_topic(clients, MAX_CLIENTS, topic, body); break;
                    default: break;
                }
                start = nl + 1;
            }

            size_t rem = (size_t)(c->buf + c->len - start);
            memmove(c->buf, start, rem);
            c->len = rem; c->buf[c->len] = '\0';
        }
    }

    net_close(lsock);
    net_cleanup();
    return 0;
}

// broker_udp_min.c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BROKER_PORT   10094
#define BUFFER_SIZE   2048
#define MAX_TOPICS    16
#define MAX_SUBS      32
#define MAX_TOPIC_LEN 128


// Defino una estructura en la que guardo todos partidos
// En esta estructura se define un partido y la lista de sus suscriptores.
typedef struct {
    struct sockaddr_in subs[MAX_SUBS]; // es una lista en la cual cada entrada es de tipo sockaddr_in, lo que significa que cada dirección pertenece a un suscriptor
    int count; // numero de suscriptores que tiene el partido
    char name[MAX_TOPIC_LEN]; //Es una cadena en la cual se guarda el nombre del partido ejemplo: BarcelonavsRealMadrid
    int used; // Si el partido esta en uso o esta libre
} Partido;


// Todos los partidos que se tienen -> es una lista con todos los partidos y sus diferentes suscriptores.
static Partido partidos[MAX_TOPICS];

// Esta función busca un partido por su nombre en la lista donde estan todos los partidos.
// Se usa para traer la estructura Partido de un partido en especfico, para si poder acceder a su lista de suscriptores con sus respectivas IPs
static int buscaPartido(const char *name) {
    for (int i = 0; i < MAX_TOPICS; ++i)
        // Compara el nombre ingresado por parametro con cada uno de los partidos y devuelve el que esta buscando
        if (partidos[i].used && strncmp(partidos[i].name, name, MAX_TOPIC_LEN) == 0)
            return i;
    // Si no lo encuentra
    return -1;
}

static int crearPartido(const char *name) {
    //Si el partido ya esta creado solo devuelve el id
    int partido = buscaPartido(name);
    if (partido >= 0) return partido;
    // Si el partido no existe lo crea
    for (int i = 0; i < MAX_TOPICS; ++i) {
        if (!partidos[i].used) {
            partidos[i].used = 1;
            partidos[i].count = 0;
            // Asigna el nombre al partido
            strncpy(partidos[i].name, name, MAX_TOPIC_LEN-1);
            // Al final del nombre del partido cierra la cadena
            partidos[i].name[MAX_TOPIC_LEN-1] = '\0';
            return i;
        }
    }
    return -1; // No se puede crear el partido porque ya hay el maximo de partidos
}

// Suscribe a un partido 
static void anadirSuscriptor(int indice_partido, const struct sockaddr_in *direccion) {
    Partido *p = &partidos[indice_partido];
    // Mira si hay suscriptores duplicados, esto no se hace porque es UDP se hace porque 
    // para publicador suscriptor es mejor no tener duplicados en los suscriptores. 
    for (int i = 0; i < p->count; ++i)
    // Revisa que no se tenga 
        if ((*p).subs[i].sin_port == (*direccion).sin_port && (*p).subs[i].sin_addr.s_addr == (*direccion).sin_addr.s_addr)
            return;
    // Crea es suscriptor con su direccíon
    if ((*p).count < MAX_SUBS)
        (*p).subs[(*p).count++] = *direccion;
}


static void transmitirActualizacion(int socketusar, int partido_transmitir, const char *actualizacion) {
    // Busca el partido del que se estan mandando actualizaciones
    Partido *p = &partidos[partido_transmitir];
    // Calcula la longitud del mensaje para usarla en el sendto()
    size_t len = strlen(actualizacion);
    // Para cada suscriptor va a enviar la respectiva actualización.
    for (int i = 0; i < (*p).count; ++i) {
        // Por cada suscriptor que tenga el partido se hace un envio. 
        // Ahora se va a enviar el mensaje a traves del socket que ya creamos al inicio.
        // La función tiene la estrcutura: 
        /*
            sszize_t sendto(int socket, const void *mensaje, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
            
            Los parametros son: 
            socket: que es el descriptor del socket creado (en este caso es la variable socketusar)
            mensaje: es un puntero al mensaje que se va a enviar (en este caso la subscripción)
            len: es la longitud del mensaje (en este caso la longitud de subscripcion)
            flags: opciones especiales del envio (en este caso no se tiene nada por lo que es 0)
            dest_addr: destino del broker que es una estructura de sockaddr.
            addtlen: es el tamaño de broker_addr.
        */

        // Lo que hace esta función es enviar la actualización al suscriptor con dirección IP y puerto definidos en broker_addr. 
         // En este caso se puede enviar sin tener conexión previa y ademas cada envió tiene una dirección de destino. 
        sendto(socketusar, actualizacion, len, 0,
               (struct sockaddr *)&(*p).subs[i], sizeof((*p).subs[i]));
    }
}

int main(void) {
    // Asigna la familia de direcciones IP y formato (datagramas) en el que se van a mandar los mensajes.
    // Hay que probar que el socker esta usando IPV4 (AF_INET) y que vamos a usar UDP (SOCK_DGRAM) 
    // lo que quiere decir que se van a enviar datagramas
    int socketusar = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketusar < 0) { 
        perror("socket"); 
        return 1;
    }

    // Defino que mi broker puede escuchar cualquier IP y usa el puerto que se definio desde el principio del laboratorio
    // Ahora se define la información que ya explicamos que se necesitan en la estructura sockaddr_in
    // Se define la familia que en este caso con AF_INET son las direcciones de IPV4.
    // Es necesario ponerlo para que se sepa que se estan usando direccion IPv4.
    // Aca si se define el puerto que es el que se definio en un principio (10094).
    // La función htons() que se traduce como "Host to Network Short", lo que hace es convertir el numero de puerto
    // y lo toma en formato big endian en donde el bit mas significativo se encuentra a la izquierda. 
    // Esto es importante ya que hay equipos que tienen un arquitectura diferente y pueden leer el numero en otro formato.
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons(BROKER_PORT);
    

    // La función bind() lo que hace es asociar un socket con una dirección local (IP y puerto)
    /* 
        Se define como int bind(int socket, const struct sockaddr *addr, socklen_t addr len);
        El primer parametro es el descriptor del socket que se creo previmente en la linea 107
        El segundo es un apuntador con la dirección de la struct sockaddr_in
        El tercer parametro es el tamaño de la estructura

        En este caso, se esta asociando el socker (socketusar) con la dirección IP que en este caso 
        se define con INADDR_ANY y el puerto 100904 que es el definido para el broker. 
        Lo que se hace es preparar el socket para que se reciban mensajes.

        (struct sockaddr *)&local -> esto se hace porque bind() espera un apuntador a struct sockaddr pero en este caso
                                    se tiene es una struct sockaddr_in por lo que se cambia el tipo de estructura. 
                                    Haciendo una revisión de esto, esto no cambia la información solo pasa el apuntador a 
                                    local como si fuera del tipo que espera la función. 
    */
    // Si no es posible realizar la asociación se cierra el socket ya que no va a ser posible usarlo para enviar mensajes.
    if (bind(socketusar, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("bind");
        close(socketusar);
        return 1;
    }

    // Se imprime el numero de puerto del broker para mayor claridad. 
    printf("Puerto del Broker: %d\n", BROKER_PORT);

    char buf[BUFFER_SIZE];

    // For infinito ya que el broker debe estar escuchando siempre
    for (;;) {
        // Guarda la IP y puerto de quien manda el mensaje. (inicialmente es 0)
        struct sockaddr_in source = {0};
        socklen_t slen = sizeof(source);
        
        // Espera por un mensaje en socketusar y escrive el mensaje recivido en el buffer
        // Llena la estructura source con la IP y puerto del publicador que envia el mensaje.
        // Ahora se tiene la función recvfrom que lo que hace es recibir datos desde el publicador. 

        /*
            ssize_t recvfrom(int socket, void *actualización, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
            
            Los parametros son: 
            socket: que es el descriptor del socket creado (en este caso es la variable socketusar)
            actualización: es un puntero a donde se van a guardar los datos que se reciban (actualización desde publicaor)
            len: es la longitud del mensaje (en este caso la longitud de actualización)
            flags: opciones especiales para recibir un mensaje (en este caso no se tiene nada por lo que es 0)
            src_addr: puntero a donde se va a guardar la dirección del broker del que viene el mensaje. 
            addrlen: es el tamaño de la dirección que se va a guardar.
        */
        ssize_t mensajeRecibido = recvfrom(socketusar, buf, sizeof(buf)-1, 0, (struct sockaddr*)&source, &slen);
        if (mensajeRecibido <= 0) continue;
        // Añade terminador para poder usar funciones de string despues
        buf[mensajeRecibido] = '\0';
        
        // Verifica que el mensaje empiece con SUBSCRIBE
        if (strncmp(buf, "SUBSCRIBE|", 10) == 0) {
            
            // Ahora se apunta al partido que se quiere
            const char *partido = buf + 10;
            if (*partido == '\0') continue;
            // Se verifica si el partido ya existe o es necesario crear uno nuevo
            int idpartido = crearPartido(partido);
            // Ya con el partido creado se añade el suscriptor que quiere recibir actualizaciones del partido. 
            if (idpartido >= 0) {
                anadirSuscriptor(idpartido, &source);
                // Se confirma la suscripción imprimiento en consola.
                printf("SUBSCRIBE '%s' (subs=%d)\n", partidos[idpartido].name, partidos[idpartido].count);
            }
        } 
        // Si lo que se quiere es recibir una actualización se debe verificar si empieza con PUBLISH lo que se esta mandando
        else if (strncmp(buf, "PUBLISH|", 8) == 0) {
            // Formato: PUBLISH|<Partido>|<Información partido>
            char *p = buf + 8;
            char *bar = strchr(p, '|');
            if (!bar) continue;
            *bar = '\0';
            const char *partido = p;
            const char *actualizacion = bar + 1;
            // Se busca el partido y se manda la actualización correspondiente al socket. 
            int idpartido = buscaPartido(partido);
            if (idpartido >= 0) {
                transmitirActualizacion(socketusar, idpartido, actualizacion);
                printf("PUBLISH '%s' → %d subs | '%s'\n", partido, partidos[idpartido].count, actualizacion);
            } else {

            }
        } else {
        }
    }

    close(socketusar);
    return 0;
}

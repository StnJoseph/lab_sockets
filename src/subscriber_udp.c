#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Defino el puerto en el que va a correr mi broker (lo escogi para que no sea un puerto bien conocido, es mi cumpleaños)
#define PUERTO 10094
// Tamaño, lo que se va a pasar son actualizaciones de los partidos son frases cortas que no ocupan mucho tamaño
#define TAMANO 2048

// El suscriptor manda un mensaje asi "SUBSCRIBE | IP del broker | Partido del que quiere saber actualizaciones"


// argc es el numero de argumentos
// argv es una lista con cada posición del mensaje
int main(int argc, char *argv[]) {
    if (argc < 3) {
        // Si no se tienen todos los argumentos necesarios se muestra el formato
        fprintf(stderr, "El formato debe ser: %s <IP BROKER> <Partido>\n", argv[0]);
        exit(1);
    }

    char *broker_ip = argv[1]; // dirección de mi broker
    char *partido = argv[2]; // partido al que me voy a suscribir


    // Se va a crear el socket que en este caso va a interactuar con el broker usando UDP y IPv4
    int socketusar = socket(AF_INET, SOCK_DGRAM, 0);

    // Hay que probar que el socker esta usando IPV4 (AF_INET) y que vamos a usar UDP (SOCK_DGRAM) 
    // lo que quiere decir que se van a enviar datagramas
    // La función socket crea un socket y devuelve un numero positivo si se crea correctamente el socket o -1 si hay un error al crearlo
    if (socketusar < 0) {
        perror("No se creo el socket");
        exit(1);
    }

    // Ahora aca se define la dirección IP y el puerto del suscriptor local. 
    // Se define una estructura de tipo sockaddr_in que representa una dirección IPV4 que es la que usamos. 
    // Se crea la variable local en el que se
    /* sockaddr_in es una estructura que se define como:

        struct sockaddr_in{
            short sin_family; es una familia de direcciones
            unsigned short sin port; es el puerto
            struct in_addr sin addr; es la dirección IP
        }
    */
    struct sockaddr_in local;

    // memset() es una función que rellena memoria con un valor especifico
    // La estructura para usarla es memset(dirección de la memoria a rellenar, valor que se quiere poner, cuantos bytes se llenan)
    // En este caso se esta haciendo para que la configuración en red quede limpia.
    // Es necesario hacer esto porque en C las variables no se inicializan automaticamente, por lo que con esta función es posible limpiar y evitar errores.
    memset(&local, 0, sizeof(local));

    // Ahora se define la información que ya explicamos que se necesitan en la estructura sockaddr_in
    // Se define la familia que en este caso con AF_INET son las direcciones de IPV4.
    // Es necesario ponerlo para que se sepa que se estan usando direccion IPv4.
    local.sin_family = AF_INET;
    // No se va a definir una dirección en especifico se va a asignar cualquiera.
    // Con esto se va definir que el socket escuche desde cualquier interfaz de red disponible.
    local.sin_addr.s_addr = INADDR_ANY;
    // Tambien se va a asignar un puerto de manera automatica, no se va a definir previamente.
    local.sin_port = 0;


    // La función bind() lo que hace es asociar un socket con una dirección local (IP y puerto)
    /* 
        Se define como int bind(int socket, const struct sockaddr *addr, socklen_t addr len);
        El primer parametro es el descriptor del socket que se creo previmente en la linea 32
        El segundo es un apuntador con la dirección de la struct sockaddr_in
        El tercer parametro es el tamaño de la estructura

        En este caso, se esta asociando el socker (socketusar) con la dirección IP que en este caso 
        se define con INADDR_ANY y el puerto 0 que es el automatico. 
        Lo que se hace es preparar el socket para que se reciban mensajes.

        (struct sockaddr *)&local -> esto se hace porque bind() espera un apuntador a struct sockaddr pero en este caso
                                    se tiene es una struct sockaddr_in por lo que se cambia el tipo de estructura. 
                                    Haciendo una revisión de esto, esto no cambia la información solo pasa el apuntador a 
                                    local como si fuera del tipo que espera la función. 
    */

    if (bind(socketusar, (struct sockaddr *)&local, sizeof(local)) < 0) {
        // En este caso bind() devuelve 1 si se puede hacer la asignación, sin embargo, puede la asignación no ocurra ya sea porque 
        // el puerto ya esta siendo usado, o la dirección IP no es valida por lo que es mejor revisar esto. 
        perror("No se pudo hacer la asignación");
        close(socketusar);
        exit(1);
    }

    // Ahora se crea una nueva estructura de tipo sockaddr_in la cual sirve para guardar la dirección IP y el puerto del broker 
    // Esto se hace ya que el broker es con quien se va a comunicar el suscriptor para pedirle las noticias de los partidos. 
    // De nuevo se tiene la estrcutura sockaddr_in la cual se define como: 
    /* 
        struct sockaddr_in{
            short sin_family; es una familia de direcciones
            unsigned short sin port; es el puerto
            struct in_addr sin addr; es la dirección IP
        }
    */
    struct sockaddr_in broker_addr;

    // De igual manera se usa memset para limpiar la estructura, asi se evitan valores 
    // que generen errores dentro de la estructura que se acaba de crear. 
    memset(&broker_addr, 0, sizeof(broker_addr));
    // Se define que el broker puede correr en una dirección IP del tipo IPv4.
    broker_addr.sin_family = AF_INET;
    // Aca si se define el puerto que es el que se definio en un principio (10094).
    // La función htons() que se traduce como "Host to Network Short", lo que hace es convertir el numero de puerto
    // y lo toma en formato big endian en donde el bit mas significativo se encuentra a la izquierda. 
    // Esto es importante ya que hay equipos que tienen un arquitectura diferente y pueden leer el numero en otro formato.
    broker_addr.sin_port = htons(PUERTO);


    // Ahora lo que se va a hacer es la dirección ip que se asigno anteriormente convertirla en binario
    // Esto se hace para que sea posible hacer envio los paquetes que en este caso son los mensajes de los partidos.
    // Se usa inet() que es una función que lo que hace es convertir una dirección IP que esta en formato de texto, 
    // a una representación binaria que es lo que se necesita. 
    // La estructura de la función inet es 
    /*
        int inet_pton(int familia, const char *source, void destino)
        Donde: 
        - familia: hace referencia a la familia de direcciones (en este caso IPv4 por lo que se usa AF_INET)
        - source: es la dirección IP en formato de texto. (La IP del broker la cual entra como parametro y se define en la linea 23)
        - destino: un apuntador hacia donde se va a guardar la versión binaria de la dirección IP (en sin_addr de la estructura broker_addr creada previamente)
        Luego int tiene tres respuestas diferentes: 
        - 1 si la conversión a binario fue exitosa. 
        - 0 si no es una dirección IP valida 
        - -1 si hay algun error en el sistema

    */
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) <= 0) {
        // Ahora aca se verifica si se tiene una dirección IP valida (se tiene un numero positivo como resultado de inet_pton)
        // Y si la ip no es valida no es posible usar el socket por lo que se decide cerrar el socket.
        perror("La dirección del broker no pudo ser asociada");
        close(socketusar);
        exit(1);
    }

    // MANDAR LA SUBSCRIPCIÓN
    // Ahora si ya se va a pasar a ver como se subscribe un subscriptor a un partido en especifico.
    // suscipcion es un arreglo en el que se va a guardar el mensaje que se va a enviar, en este caso, el maximo tamaño
    // del mensaje puede ser hasta 2048, lo que significa que puede tener hasta 2047 caracteres.
    char subscripcion[TAMANO];
    // Ahora se escribe el mensaje que entra en el arreglo que se acaba de crear (subscripción)
    // Se usa la función snprintf la cual tiene la estructura.
    /* 
        El primer parametro es el arreglo en el que se va a escribir (subscripcion)
        El segundo parametro es el tamaño del texto maximo que se puede escribir. 
        El tercer parametro es el texto que se va a escribir. 
    */
    // En este caso, el mensaje que se va a mandar es "SUBSCRIBE|Partido al que se va a subscribir (Partido 1, Partido 2, Partido 3)"
    snprintf(subscripcion, sizeof(subscripcion), "SUBSCRIBE|%s", partido);

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

    // Lo que hace esta función es enviar la subscripcion al socket con dirección IP y puerto definidos en broker_addr. 
    // En este caso se puede enviar sin tener conexión previa y ademas cada envió tiene una dirección de destino. 
    sendto(socketusar, subscripcion, strlen(subscripcion), 0, (struct sockaddr *)&broker_addr, sizeof(broker_addr));
    // Se manda una confirmación de la subscripción al partido
    printf("Se ha subscrito al partido: '%s'.\n", partido);

    // RECIBIR ACTUALIZACIONES DEL PARTIDO

    // Se hace un bucle infinito ya que se quieren recibir actualizaciones siempre o hasta que ocurra un error.
    // Lo que hace es esperar hasta que lleguen nuevas actualizaciones. 
    // Es un siclo infinito ya que UDP no tiene una conexión persistente y cada mensaje llega de forma independiente,
    // Por lo que el subscriptor debe estar escuchando de manera constante.
    while (1) {
        // Estructura en donde se define el tamaño de la respuesta, en este caso igual es de 2047 caracteres ya que
        // las actualizaciones de partido no son muy largas. 
        char actualizacion[TAMANO];

        // Se crea una nueva estructura del tipo sockaddr_in para guardar la dirección del broker del que se esta recibiendo
        // el mensaje. Asi se puede saber de donde recibir el mensaje.  
        struct sockaddr_in source;
        // Luego se guarda el tamaño de la dirección para que pueda ser usada posteriormente, sobre cuanto espacio se necesita para guardar 
        // la dirección del broker del que se esta enviando.
        socklen_t sourcelen = sizeof(source);

        // Ahora se tiene la función recvfrom que lo que hace es recibir datos desde un socket. 

        /*
            ssize_t recvfrom(int socket, void *actualización, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
            
            Los parametros son: 
            socket: que es el descriptor del socket creado (en este caso es la variable socketusar)
            actualización: es un puntero a donde se van a guardar los datos que se reciban (actualización)
            len: es la longitud del mensaje (en este caso la longitud de actualización)
            flags: opciones especiales para recibir un mensaje (en este caso no se tiene nada por lo que es 0)
            src_addr: puntero a donde se va a guardar la dirección del broker del que viene el mensaje. 
            addrlen: es el tamaño de la dirección que se va a guardar.
        */
        int noticia = recvfrom(socketusar, actualizacion, TAMANO - 1, 0,
                         (struct sockaddr *)&source, &sourcelen);

        actualizacion[noticia] = '\0';
        // Se imprime la actualización que del partido al que se esta subscrito
        printf("Actualización del partido: %s\n", actualizacion);
    }

    // Cerrar el socket
    // En este caso, lo que hace es que el descriptor del socket que se creaba con la función socket()
    // y que en este caso se llama socket usar. Lo que hace itnernamente es que marca es descriptor creado
    // previamente como cerrado para que no se pueda usar mas. Y se liberan los recursos que se estaban utilizando. 
    close(socketusar);
    return 0;
}

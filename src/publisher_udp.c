
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Puerto donde esta definido el broker, que se definio previamente 
#define PUERTO 10094

//Se define el tamaño del mensaje en este caso del mensaje que se va a mandar.
#define TAMANO 2048

// El publicador manda un mensaje asi "PUBLISH | IP del broker | Partido al que va a mandar actualizaciones"



// argc es el numero de argumentos
// argv es una lista con cada posición del mensaje
int main(int argc, char *argv[]) {

    if (argc < 3) {
        // Si no se tienen todos los argumentos necesarios se muestra el formato
        fprintf(stderr, "El formato debe ser: %s <IP BROKER> <Partido>\n", argv[0]);
        exit(1);
    }

    //

    const char *broker_ip = argv[1]; // dirección de mi broker (10094)
    const char *partido   = argv[2]; // partido al que me voy a suscribir

    // Se va a crear el socket que en este caso va a interactuar con el broker usando UDP y IPv4
    int socketusar = socket(AF_INET, SOCK_DGRAM, 0);

    // Hay que probar que el socker esta usando IPV4 (AF_INET) y que vamos a usar UDP (SOCK_DGRAM) 
    // lo que quiere decir que se van a enviar datagramas
    // La función socket crea un socket y devuelve un numero positivo si se crea correctamente el socket o -1 si hay un error al crearlo
    if (socketusar < 0) {
        perror("No se creo el socket");
        exit(1);
    }

    // Ahora aca se define la dirección IP y el puerto del broker. 
    // Se define una estructura de tipo sockaddr_in que representa una dirección IPV4 que es la que usamos. 
    // Se crea la variable local en el que se
    /* sockaddr_in es una estructura que se define como:

        struct sockaddr_in{
            short sin_family; es una familia de direcciones
            unsigned short sin port; es el puerto
            struct in_addr sin addr; es la dirección IP
        }
    */
    struct sockaddr_in broker_addr;

    // memset() es una función que rellena memoria con un valor especifico
    // La estructura para usarla es memset(dirección de la memoria a rellenar, valor que se quiere poner, cuantos bytes se llenan)
    // En este caso se esta haciendo para que la configuración en red quede limpia.
    // Es necesario hacer esto porque en C las variables no se inicializan automaticamente, por lo que con esta función es posible limpiar y evitar errores.
    memset(&broker_addr, 0, sizeof(broker_addr));

    // Ahora se define la información que ya explicamos que se necesitan en la estructura sockaddr_in
    // Se define la familia que en este caso con AF_INET son las direcciones de IPV4.
    // Es necesario ponerlo para que se sepa que se estan usando direccion IPv4.
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

    // Necesito el len de la dirreción ip del broker para poder usarla en la función sendto()
    socklen_t brokerdirlen = sizeof(broker_addr);

    // MANDAR PUBLICACIÓN
   

    // Se van a enviar 20 actualizaciones de partidos para 
    for (int i = 1; i <= 20; ++i) {
        // Ahora se tiene la manera en la que un publicador empieza a publicar actualizaciones sobre los partidos.
        // En actualizacion se tiene un arreglo en el que se va a guardar el mensaje que se va a enviar, en este caso, el maximo tamaño
        // del mensaje puede ser hasta 2048, lo que significa que puede tener hasta 2047 caracteres. Esto sale del tamaño maximo que
        // Se definio en un prinicipio para los caracteres.
        char actualizacion[TAMANO];
        int minuto = rand() % 90 + 1; // El partido dura 90 minutos

    // Ahora se escribe el mensaje que entra en el arreglo que se acaba de crear (subscripción)
    // Se usa la función snprintf la cual tiene la estructura.
    /* 
        El primer parametro es el arreglo en el que se va a escribir (subscripcion)
        El segundo parametro es el tamaño del texto maximo que se puede escribir. 
        El tercer parametro es el texto que se va a escribir. 
    */
    // En este caso, el mensaje que se va a mandar es "SUBSCRIBE|Partido al que se va a subscribir (Partido 1, Partido 2, Partido 3)"
        snprintf(actualizacion, sizeof(actualizacion), "PUBLISH|%s|Gol al minuto %d (%d)", partido, minuto, i);

        // Ahora se va a enviar el mensaje a traves del socket que ya creamos al inicio.
    // La función tiene la estrcutura: 
        /*
            sszize_t sendto(int socket, const void *mensaje, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
            
            Los parametros son: 
            socket: que es el descriptor del socket creado (en este caso es la variable socketusar)
            mensaje: es un puntero al mensaje que se va a enviar (en este caso la publicación)
            len: es la longitud del mensaje (en este caso la longitud de publicación)
            flags: opciones especiales del envio (en este caso no se tiene nada por lo que es 0)
            dest_addr: destino del broker que es una estructura de sockaddr.
            addtlen: es el tamaño de broker_addr.
        */

    // Lo que hace esta función es enviar la subscripcion al socket con dirección IP y puerto definidos en broker_addr. 
    // En este caso se puede enviar sin tener conexión previa y ademas cada envió tiene una dirección de destino. 
        sendto(socketusar, actualizacion, strlen(actualizacion), 0,
               (struct sockaddr *)&broker_addr, brokerdirlen);

        // Se imprime la actualización del partido para que el publicador pueda ver que actualizaciones ha mandado anteriormente.
        printf("Actualización de partido: %s\n", actualizacion);

    }

    // Cerrar el socket
    // En este caso, lo que hace es que el descriptor del socket que se creaba con la función socket()
    // y que en este caso se llama socketusar. Lo que hace internamente es que marca es descriptor creado
    // previamente como cerrado para que no se pueda usar mas. Y se liberan los recursos que se estaban utilizando. 
    close(socketusar);
    return EXIT_SUCCESS;
}

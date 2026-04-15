#include "utils.h"

t_log* logger; //chat dic

int iniciar_servidor(void)
{

    int err;
    struct addrinfo hints, *servinfo;

    // Validar que el logger esté inicializado
    if (!logger) {
        fprintf(stderr, "Logger no inicializado\n");
        abort();
    }

    //configuracion de hints

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // obtener info de la direccion
    // Usamos "4444" directamente si la constante PUERTO no está definida
    err = getaddrinfo(NULL, "4444", &hints, &servinfo);
    if (err){
        log_error(logger, "Error en getaddrinfo: %s", gai_strerror(err)); //esto noc bien
        abort();
    }

    //crear el socket

    int socket_servidor = socket(servinfo->ai_family,
                        servinfo->ai_socktype,
                        servinfo->ai_protocol);
    
    if(socket_servidor == -1){
        perror("Error al crear el socket");
        log_error(logger, "Error al crear el socket");
        abort();
    }

    // Para evitar el error de "Address already in use"
    int refuse = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    if (err == -1) {
        perror("Error en bind");
        abort();
    }

    err = listen(socket_servidor, SOMAXCONN);
    if(err == -1){
        perror("Error en listen");
        log_error(logger, "Error en listen");
        abort();
    }

    freeaddrinfo(servinfo);
    log_trace(logger, "Listo para escuchar a mi cliente");

    return socket_servidor;
}

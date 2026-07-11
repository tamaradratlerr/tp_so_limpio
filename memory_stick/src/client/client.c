#include "client.h"

extern bool mock;
int socket_km;

void arrancar_cliente_km(void) {

    char* ip_km = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_km = config_get_string_value(config, "KERNEL_MEMORY_PORT");
    char* ip = config_get_string_value(config, "IP_MEMORY_STICK");
    char* port = config_get_string_value(config, "MEMORY_STICK_PORT");


    if(mock){

        log_info(logger,
            "## Conectado a Kernel Memory (MOCK)");

        return;
    }


    socket_km = crear_conexion(
        ip_km,
        puerto_km,
        logger,
        KERNEL_MEMORY
    );


    if(socket_km < 0){

        log_error(logger,
            "Error: No se pudo conectar a Kernel Memory");

        exit(EXIT_FAILURE);
    }


    log_info(logger,
        "## Conectado a Kernel Memory");

    enviar_op_code(NUEVA_MEMORY_STICK,socket_km);

    enviar_int(ms_globals.tamanio,socket_km);

    enviar_mensaje(ip,socket_km);

    enviar_mensaje(port,socket_km);


    log_info(logger,
    "Enviando a Kernel Memory: tamaño=%u",
    ms_globals.tamanio);


    pthread_t thread_km;

    int* socket_km_ptr = malloc(sizeof(int));
    *socket_km_ptr = socket_km;


    pthread_create(
        &thread_km,
        NULL,
        atender_kernel_memory,
        socket_km_ptr
    );


    pthread_detach(thread_km);

}

void* atender_kernel_memory(void* arg) {

    int socket_km = *(int*)arg;
    free(arg);

    while(1){

        op_code cop = recibir_op_code(socket_km);

        if(cop == -1){
            break;
        }

        switch(cop){

            case DESCONEXION_MS:
                log_info(logger, "Kernel Memory solicita desconexion");
                close(socket_km);
                return NULL;

            default:
                log_warning(logger, "Operacion no soportada recibida de KM: %d", cop);
                break;
        }
    }

    close(socket_km);
    return NULL;
}
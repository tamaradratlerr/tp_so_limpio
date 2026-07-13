#include "client.h"
#include "../server/server.h"

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

            case ESCRIBIR_MEMORIA: {

                uint32_t dir_fisica = recibir_int(socket_km);
                uint32_t tamanio = recibir_int(socket_km);

                void* datos_escribir = malloc(tamanio);

                if (recv(socket_km,
                        datos_escribir,
                        tamanio,
                        MSG_WAITALL) != tamanio) {

                    log_error(logger, "Error recibiendo datos de escritura");
                    free(datos_escribir);
                    break;
                }

                uint32_t dir_local = dir_fisica - ms_globals.base;

                escribir_en_bloque_memoria(
                    dir_local,
                    datos_escribir,
                    tamanio
                );

                free(datos_escribir);

                enviar_op_code(OK, socket_km);
                break;
            }

            case LEER_MEMORIA:
            {
                uint32_t dir_fisica;
                uint32_t tamanio;

                recv(socket_km,
                    &dir_fisica,
                    sizeof(uint32_t),
                    MSG_WAITALL);

                recv(socket_km,
                    &tamanio,
                    sizeof(uint32_t),
                    MSG_WAITALL);

                uint32_t dir_local = dir_fisica - ms_globals.base;

                log_debug(logger, "antes de leer");

                void* bytes = leer_de_bloque_memoria(
                    dir_local,
                    tamanio
                );

                unsigned char* p = bytes;

                printf("bytes=%p\n", bytes);

                for(int i=0;i<tamanio;i++)
                    printf("%02X ", p[i]);

                printf("\n");
                fflush(stdout);

                send(socket_km,
                    bytes,
                    tamanio,
                    0);

                log_debug(logger, "despues del send");

                free(bytes);
                
                log_debug(logger, "despues del free");

            }

            default:
                log_warning(logger, "Operacion no soportada recibida de KM: %d", cop);
                break;
        }
    }

    close(socket_km);
    return NULL;
}

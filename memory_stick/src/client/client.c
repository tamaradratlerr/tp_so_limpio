#include "client.h"


void arrancar_cliente_km(void) {
    char* ip_km = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto_km = config_get_string_value(config, "KERNEL_MEMORY_PORT");

    int socket_km = crear_conexion(ip_km, puerto_km, logger, KERNEL_MEMORY);
    if (socket_km < 0) {
        log_error(logger, "Error: No se pudo conectar a la Kernel Memory. Abortando.");
        exit(EXIT_FAILURE);
    }
    log_info(logger, "## Conectado a Kernel Memory");

    t_paquete* p_handshake = crear_paquete(NUEVA_MEMORY_STICK); 
    agregar_a_paquete(p_handshake, &(ms_globals.memory_size), sizeof(uint32_t));
    enviar_paquete(p_handshake, socket_km);
    eliminar_paquete(p_handshake);

    pthread_t thread_km;
    int* socket_km_ptr = malloc(sizeof(int));
    *socket_km_ptr = socket_km;
    
    pthread_create(&thread_km, NULL, atender_kernel_memory, socket_km_ptr);
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
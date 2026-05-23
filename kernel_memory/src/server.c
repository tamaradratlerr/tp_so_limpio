#include "utils.h"
#include <sys/socket.h>
#include <pthread.h>
#include <commons/config.h>

t_log* logger;
t_config* config;

void atender_cliente(void* arg) {
    int cliente_fd = *(int*)arg;
    free(arg);

    while (1) {
        int cod_op = recibir_operacion(cliente_fd);
        
        switch (cod_op) {
            case NUEVA_CPU:
                log_info(logger, "CPU detectada. Confirmando Handshake...");
                int ok = 1;
                send(cliente_fd, &ok, sizeof(int), 0); 
                break;

            case SOLICITUD_INSTRUCCION:
                manejar_pedido_instruccion_cpu(cliente_fd);
                break;

            case LEER_MEMORIA:
                manejar_lectura_memoria(cliente_fd); 
                break;

            case ESCRIBIR_MEMORIA:
                manejar_escritura_memoria(cliente_fd);
                break;

            case km_GUARDAR_CONTEXTO:
                manejar_guardar_contexto(cliente_fd);
                break;

            // Espacio libre para cuando agregues los del Kernel más adelante:
            // case CREAR_PROCESO: manejar_crear_proceso(cliente_fd); break;
            // case FINALIZAR_PROCESO: manejar_finalizar_proceso(cliente_fd); break;

            case -1:
                log_error(logger, "Desconexion en socket %d", cliente_fd);
                close(cliente_fd);
                return;

            default:
                log_warning(logger, "Op desconocida (%d) en socket %d", cod_op, cliente_fd);
                break;
        }
    }
}

int main(void) {
    config = config_create("kernel_memory.config");
    if (config == NULL) {
        perror("No se pudo leer el config");
        return 1;
    }

    char* nivel_log = config_get_string_value(config, "LOG_LEVEL");
    logger = log_create("kernel_memory.log", "KERNEL_MEMORY", 1, log_level_from_string(nivel_log));

    log_info(logger, "Config cargado. Estrategia: %s", config_get_string_value(config, "ALLOCATION_STRATEGY"));

    inicializar_utils();

    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    int server_fd = iniciar_servidor(puerto);
    log_info(logger, "Kernel Memory escuchando en puerto %s", puerto);

    // Bucle de conexiones (Multihilo)
    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        log_info(logger, "### Nueva conexion detectada ###");

        pthread_t hilo;
        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = cliente_fd;

        pthread_create(&hilo, NULL, (void*)atender_cliente, socket_ptr);
        pthread_detach(hilo);
    }

    config_destroy(config);
    return 0;
}
#include "utils.h"

t_log* logger;
t_config* config;

void imprimir_paquete(char* valor) {
    log_info(logger, "Dato del paquete: %s", valor);
}

void atender_cliente(void* arg) {
    int cliente_fd = *(int*)arg;
    free(arg);

    while (1) {
        int cod_op = recibir_operacion(cliente_fd);
        switch (cod_op) {
            case MENSAJE:
                recibir_mensaje(cliente_fd);
                break;
            case PAQUETE:
                t_list* lista = recibir_paquete(cliente_fd);
                log_info(logger, "Paquete recibido de socket %d:", cliente_fd);
                list_iterate(lista, (void*)imprimir_paquete);
                list_destroy_and_destroy_elements(lista, free);
                break;
            case -1:
                log_error(logger, "Desconexion en socket %d", cliente_fd);
                return;
            default:
                log_warning(logger, "Op desconocida en socket %d", cliente_fd);
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


    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    int server_fd = iniciar_servidor(puerto);
    log_info(logger, "Kernel Memory escuchando en puerto %s", puerto);

    // 4. Bucle de conexiones (Multihilo)
    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        log_info(logger, "### Nueva conexion detectada ###");

        pthread_t hilo;
        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = cliente_fd;

        pthread_create(&hilo, NULL, (void*)atender_cliente, socket_ptr);
        pthread_detach(hilo);
    }

    return 0;
}

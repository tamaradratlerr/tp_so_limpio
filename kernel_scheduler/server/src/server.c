#include "server.h"

int main(void) {
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    int server_fd = iniciar_servidor();
    log_info(logger, "Servidor listo para recibir clientes");

    // BUCLE 1: Para aceptar nuevos clientes uno tras otro
    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar un cliente");
            continue; // Intentamos con el siguiente
        }
        
        log_info(logger, "Cliente conectado. Iniciando recepción...");

        // BUCLE 2: El que ya tenías, para procesar a ESTE cliente
        int control_loop = 1;
        while (control_loop) {
            int cod_op = recibir_operacion(cliente_fd);
            
            t_list* lista;
            switch (cod_op) {
                case MENSAJE:
                    recibir_mensaje(cliente_fd);
                    break;
                case PAQUETE:
                    lista = recibir_paquete(cliente_fd);
                    log_info(logger, "Me llegaron los siguientes valores:");
                    list_iterate(lista, (void*) iterator);
                    list_destroy_and_destroy_elements(lista, (void*)free);
                    break;
                case -1:
                    log_info(logger, "El cliente se desconectó.");
                    control_loop = 0; // Salimos del bucle interno
                    break;
                default:
                    log_warning(logger,"Operación desconocida.");
                    break;
            }
        }
        
        // Al terminar el bucle de mensajes, cerramos el socket de ese cliente
        close(cliente_fd);
        log_info(logger, "Esperando al siguiente cliente...");
    }

    return EXIT_SUCCESS;
}

void iterator(char* value) {
	log_info(logger,"%s", value);
}

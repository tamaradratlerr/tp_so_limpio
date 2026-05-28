#include "utils.h"
#include <sys/socket.h>
#include <pthread.h>
#include <commons/config.h>

t_log* logger;
t_config* config;

void atender_cpu(int cpu_fd) {
    log_info(logger, "--- Hilo CPU [%d] iniciado ---", cpu_fd);
    while (1) {
        int cod_op = recibir_operacion(cpu_fd);
        
        switch (cod_op) {
            case SOLICITUD_INSTRUCCION:
                manejar_pedido_instruccion_cpu(cpu_fd);
                break;
                
            case ENVIAR_PROCESO:
            //rescibir proceso de ks
                recibir_proceso();
                break;

            case LEER_MEMORIA:
                manejar_lectura_memoria(cpu_fd); 
                break;

            case ESCRIBIR_MEMORIA:
                manejar_escritura_memoria(cpu_fd);
                break;

            case km_GUARDAR_CONTEXTO:
                manejar_guardar_contexto(cpu_fd);
                break;

            case -1:
                log_warning(logger, "CPU en socket %d se desconectó.", cpu_fd);
                close(cpu_fd);
                return;

            default:
                log_error(logger, "Opcode desconocido (%d) enviado por CPU.", cod_op);
                break;
        }
    }
}

void atender_kernel(int kernel_fd) {
    log_info(logger, "--- Hilo KERNEL SCHEDULER [%d] iniciado ---", kernel_fd);
    while (1) {
        int cod_op = recibir_operacion(kernel_fd);
        
        switch (cod_op) {
            case ks_INIT_PROC: 
                manejar_crear_proceso(kernel_fd); 
                break;

            case ks_EXIT: 
                manejar_finalizar_proceso(kernel_fd); 
                break;

            case -1:
                log_warning(logger, "Kernel (ks) en socket %d se desconectó.", kernel_fd);
                close(kernel_fd);
                return;

            default:
                log_error(logger, "Opcode desconocido (%d) enviado por Kernel.", cod_op);
                break;
        }
    }
}
void atender_cliente_inicial(void* arg) {
    int cliente_fd = *(int*)arg;
    free(arg); 

    int handshake_op = recibir_operacion(cliente_fd);
    int respuesta_ok = 1;

    switch (handshake_op) {
        
        case NUEVA_CPU: 
            log_info(logger, "## Handshake recibido: CPU detectada en socket %d. Confirmando...", cliente_fd);
            send(cliente_fd, &respuesta_ok, sizeof(int), 0); 
            
        
            atender_cpu(cliente_fd);
            break;

        case NUEVO_KERNEL: 
            log_info(logger, "## Handshake recibido: KERNEL (ks) detectado en socket %d. Confirmando...", cliente_fd);
            send(cliente_fd, &respuesta_ok, sizeof(int), 0); 
            
            atender_kernel(cliente_fd);
            break;

        case -1:
            log_error(logger, "El cliente se desconectó en el intento de Handshake inicial.");
            close(cliente_fd);
            break;

        default:
            log_warning(logger, "Cliente desconocido intentó conectarse (Opcode: %d). Cerrando conexión.", handshake_op);
            close(cliente_fd);
            break;
    }
}

int main(void) {

    config = config_create("kernel_memory.config");
    if (config == NULL) {
        perror("No se pudo leer el archivo kernel_memory.config");
        return 1;
    }

    char* nivel_log = config_get_string_value(config, "LOG_LEVEL");
    logger = log_create("kernel_memory.log", "KERNEL_MEMORY", 1, log_level_from_string(nivel_log));

    log_info(logger, "Kernel Memory Inicializado de forma correcta.");

    inicializar_utils();

    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    int server_fd = iniciar_servidor(puerto);
    log_info(logger, "Kernel Memory escuchando en puerto %s...", puerto);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        
        if (cliente_fd != -1) {
            log_info(logger, "Conexión entrante detectada en socket %d. Creando hilo de identificación...", cliente_fd);

            pthread_t hilo_cliente;
            int* socket_ptr = malloc(sizeof(int));
            *socket_ptr = cliente_fd;

            // Creamos el hilo genérico que primero resolverá el Handshake
            pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_inicial, socket_ptr);
            pthread_detach(hilo_cliente);
        }
    }

    config_destroy(config);
    log_destroy(logger);
    return 0;
}
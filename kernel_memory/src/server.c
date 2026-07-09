#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include <string.h> // strdup vive aquí
#include "utils.h"
#include <sys/socket.h>
#include <pthread.h>
#include <commons/config.h>

t_log* logger;
t_config* config;

// FALTA LLENAR HANDSHAKE : 7/6

int main(int argc, char** argv) {

    // Justo antes de la línea del config_create
    printf("DEBUG: Intentando abrir el archivo en la ruta: %s\n", argv[1]);
    // Obtener el directorio de trabajo actual
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("DEBUG: Directorio actual de ejecución: %s\n", cwd);
    }

    if (argc < 2) {
        printf("Falta el path al archivo de config\n");
        return 1;
    }

    config = config_create(argv[1]); // <--- Usa el argumento que pasas por consola
    if (config == NULL) {
        perror("Error al abrir config"); 
        return 1;
    }

    char* nivel_log = config_get_string_value(config, "LOG_LEVEL");
    logger = log_create("kernel_memory.log", "KERNEL_MEMORY", 1, log_level_from_string(nivel_log));

    log_info(logger, "Kernel Memory Inicializado de forma correcta.");

    inicializar_utils();

    char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    int server_fd = iniciar_servidor(puerto, logger);
    log_info(logger, "Kernel Memory escuchando en puerto %s...", puerto);

    while (1) {
            int cliente_fd = esperar_cliente(server_fd, logger);
        
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



void atender_cpu(int cpu_fd) {
    log_info(logger, "--- Hilo CPU [%d] iniciado ---", cpu_fd);
    while (1) {
        int cod_op = recibir_op_code(cpu_fd);
        
        switch (cod_op) {
            case SOLICITUD_INSTRUCCION:
                manejar_pedido_instruccion_cpu(cpu_fd);
                break;
                
            case ENVIAR_PROCESO:
            //rescibir proceso de ks
                t_list* paquete = recibir_paquete(cpu_fd); // O la variable de socket que uses
            // Y luego procesa ese paquete
                list_destroy_and_destroy_elements(paquete, free);
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
        int cod_op = recibir_op_code(kernel_fd);
        
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
void* atender_cliente_inicial(void* arg) {
    int cliente_fd = *(int*)arg;
    free(arg); 

    int handshake_op = recibir_op_code(cliente_fd);
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

        case NUEVA_MEMORY_STICK:
            log_info(logger, "## Handshake recibido: MEMORY STICK detectada en socket %d. Registrando...", cliente_fd);
            conexion_memory_stick(cliente_fd);
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

    return NULL;

}







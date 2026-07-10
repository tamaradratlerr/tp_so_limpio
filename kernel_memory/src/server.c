#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include <string.h> 
#include "utils.h"
#include <sys/socket.h>
#include <pthread.h>
#include <commons/config.h>
#include <commons/bitarray.h> // Necesaria para gestionar bloques
#include <math.h>            // Necesaria para ceil()

// --- Variables Globales ---
t_log* logger;
t_config* config_km;
int socket_swap;             // Socket para hablar con el SWAP
int block_size_swap;         // Tamaño de bloque recibido del SWAP
t_bitarray* bitmap_swap;     // Administrador de bloques libres
int total_bloques_swap;
int socket_kernel_scheduler = -1;

/*MAIN*/
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Falta el path al archivo de config\n");
        return 1;
    }

    config_km = config_create(argv[1]);
    if (config_km == NULL) {
        perror("Error al abrir config"); 
        return 1;
    }

    char* nivel_log = config_get_string_value(config_km, "LOG_LEVEL");
    logger = log_create("kernel_memory.log", "KERNEL_MEMORY", 1, log_level_from_string(nivel_log));

    log_info(logger, "Kernel Memory Inicializado de forma correcta.");

    inicializar_utils();


    // --- INICIAMOS EL SERVIDOR ---
    char* puerto = config_get_string_value(config_km, "PUERTO_ESCUCHA");
    int server_fd = iniciar_servidor(puerto, logger);
    log_info(logger, "Kernel Memory escuchando en puerto %s...", puerto);

    while (1) {
        int cliente_fd = esperar_cliente(server_fd, logger);
        
        if (cliente_fd != -1) {
            log_info(logger, "Conexión entrante detectada en socket %d. Creando hilo...", cliente_fd);

            pthread_t hilo_cliente;
            int* socket_ptr = malloc(sizeof(int));
            *socket_ptr = cliente_fd;

            // Aquí el cliente (sea SWAP, CPU o Kernel) enviará su OpCode de Handshake
            pthread_create(&hilo_cliente, NULL, (void*)atender_cliente_inicial, socket_ptr);
            pthread_detach(hilo_cliente);
        }
    }

    config_destroy(config_km);
    log_destroy(logger);
    return 0;
}


/*FUNCIONES*/
void atender_cpu(int cpu_fd) {
    log_info(logger, "--- Hilo CPU [%d] iniciado ---", cpu_fd);
    
    while (1) {
        
        log_info(logger,"*****     Esperando Por nuevas Solicitudes de CPU     *****");
        int cod_op = recibir_op_code(cpu_fd);
        
        switch (cod_op) {
            
            case CONTEXTO: 

                log_info(logger, "CPU solicita contexto");

                int pid_ejecutando = recibir_pid(cpu_fd);

                enviar_contexto_cpu(cpu_fd, pid_ejecutando);

                log_debug(logger, "FIN Enviar Contexto");

                break;
            
            case cpu_GUARDAR_CONTEXTO:

                log_debug(logger, "CPU manda para guardar contexto");

                recibir_contexto_cpu(cpu_fd);

                break;

            case FETCH:
                manejar_pedido_instruccion_cpu(cpu_fd);
                
                break;
                
            case ENVIAR_PROCESO:
                //rescibir proceso de ks
                t_list* paquete = recibir_paquete(cpu_fd); // O la variable de socket que uses
                // Y luego procesa ese paquete
                list_destroy_and_destroy_elements(paquete, free);
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

        log_info(logger,"*****     Esperando Por nuevas Solicitudes de Kernel Scheduler     *****");

        int cod_op = recibir_op_code(kernel_fd);

        switch (cod_op) 
        {
            case ENVIAR_PROCESO:
            
            case ks_INIT_PROC:
                manejar_crear_proceso(kernel_fd);
                break;

            case LEER_MEMORIA:
                lectura_memoria(kernel_fd);
                break;

            case ESCRIBIR_MEMORIA:
                escritura_memoria(kernel_fd);
                break;
           
            case SUSPENDIDO: 
                
                int pid_s = recibir_pid(kernel_fd);

                    suspender_proceso(pid_s);
                    enviar_op_code(OK, kernel_fd);
                    break;

            case NUEVO_ESPACIO:  //desuspendido
                
                int pid_e = recibir_pid(kernel_fd);

                if (desuspender_proceso(pid_e) == 0) 
                {
                    enviar_op_code(OK, kernel_fd);
                } 
                else 
                {
                    enviar_op_code(NOTOK, kernel_fd);
                }
                
                break;

            case gl_MEM_ALLOC:

                int pid_a = recibir_pid(kernel_fd);
                int id_segmento = recibir_int(kernel_fd); 
                int tamanio = recibir_int(kernel_fd);

                creacion_segmento(kernel_fd,socket_kernel_scheduler,pid_a,id_segmento,tamanio);

                break;
            
            case gl_MEM_FREE:

                int pid_mem = recibir_pid (kernel_fd);
                int id_segmento_mem = recibir_int (kernel_fd);

                eliminar_segmento(pid_mem, id_segmento_mem);
                enviar_op_code(OK, kernel_fd);

                break;

            case gl_EXIT: 
                manejar_finalizar_proceso(kernel_fd);
                break;

            case km_IO_STDOUT:
                lectura_memoria(kernel_fd);
                break;

            case km_IO_STDIN:
                escritura_memoria(kernel_fd);
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

void* atender_cliente_inicial(void* arg) 
{
    
    int cliente_fd = *(int*) arg;
    free(arg);

    int handshake_op = recibir_op_code(cliente_fd);
    int respuesta_ok = OK;

    switch (handshake_op) 
    {
        case HANDSHAKE_SWAP: 
            log_info(logger, "## SWAP detectado. Recibiendo configuración...");

            t_list* paquete_swap = recibir_paquete(cliente_fd);

            if (paquete_swap == NULL || list_size(paquete_swap) < 2) {
                log_error(logger, "## Error: handshake de SWAP inválido");

                if (paquete_swap != NULL) {
                    list_destroy_and_destroy_elements(paquete_swap, free);
                }

                close(cliente_fd);
                return NULL;
            }

            block_size_swap = *(int*) list_get(paquete_swap, 0);
            int total_size_swap = *(int*) list_get(paquete_swap, 1);

            list_destroy_and_destroy_elements(paquete_swap, free);

            if (
                block_size_swap <= 0 ||
                total_size_swap <= 0 ||
                total_size_swap % block_size_swap != 0
            ) {
                log_error(
                    logger,
                    "## Error: configuración de SWAP inválida. BLOCK_SIZE=%d TOTAL_SIZE=%d",
                    block_size_swap,
                    total_size_swap
                );

                close(cliente_fd);
                return NULL;
            }

            socket_swap = cliente_fd;
            total_bloques_swap = total_size_swap / block_size_swap;

            char* bitarray_data = calloc(1, ceil((double) total_bloques_swap / 8));
            bitmap_swap = bitarray_create_with_mode(
                bitarray_data,
                total_bloques_swap,
                LSB_FIRST
            );

            log_info(
                logger,
                "## SWAP conectado - Block Size: %d - Total Size: %d - Bloques: %d",
                block_size_swap,
                total_size_swap,
                total_bloques_swap
            );

            return NULL;
            break;
        

        case NUEVA_CPU:
            log_info(logger, "## Handshake recibido: CPU detectada en socket %d. Confirmando...", cliente_fd);
            enviar_op_code(OK,cliente_fd);

            atender_cpu(cliente_fd);
            break;

        case NUEVO_KERNEL:
            log_info(logger, "## Handshake recibido: KERNEL (ks) detectado en socket %d. Confirmando...", cliente_fd);
            enviar_op_code(OK,cliente_fd);
            socket_kernel_scheduler = cliente_fd;

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







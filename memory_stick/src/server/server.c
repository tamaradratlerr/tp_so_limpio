#include "utils.h"

bool mock = true;
t_log* logger;
t_config* config;
t_memory_stick_globals ms_globals;
int delay_memoria;
pthread_mutex_t mutex_memoria;

extern void arrancar_cliente_km(void);


int main(int argc, char** argv)
{
    validar_argumentos(argc, argv);

    config = config_create(argv[1]);
    
    if(config == NULL){
        printf("No se pudo abrir el archivo de configuración: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    char* file          = config_get_string_value(config, "SERVER_LOG_NAME");
    char* process_name  = config_get_string_value(config, "PROCCES_NAME"); 
    char* server_port   = config_get_string_value(config, "MEMORY_STICK_PORT");

    delay_memoria = config_get_int_value(config, "MEMORY_DELAY");

    uint32_t tamanio_ms = (uint32_t)atoi(argv[2]);


    logger = log_create(file, process_name, 1, LOG_LEVEL_DEBUG);

    if(logger == NULL){
        printf("ERROR: No se pudo crear logger\n");
        abort();
    }


    init_memory_stick(tamanio_ms);

    pthread_mutex_init(&mutex_memoria, NULL);


    // Conexión con Kernel Memory
    arrancar_cliente_km();


    // Servidor para CPUs
    int server_fd = iniciar_servidor(server_port, logger);

    log_info(logger, "Servidor MEMORY_STICK listo para recibir clientes");


    while(1){

        int socket_cliente = esperar_cliente(server_fd, logger);


        pthread_t thread;

        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_cliente;


        if(pthread_create(&thread, NULL, atender_cliente, socket_ptr) != 0){

            perror("pthread_create");

            close(socket_cliente);
            free(socket_ptr);

            continue;
        }


        pthread_detach(thread);
    }


    config_destroy(config);
    pthread_mutex_destroy(&mutex_memoria);
    free_all_globals();

    return EXIT_SUCCESS;
}


void validar_argumentos(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Error, faltan argumentos. \nUso: %s <Archivo Config> <Tamaño Bytes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

void escribir_en_bloque_memoria(uint32_t dir_fisica, void* datos_a_escribir, uint32_t tamanio) {
    usleep(delay_memoria * 1000);

    pthread_mutex_lock(&mutex_memoria);
    memcpy(ms_globals.memory + dir_fisica, datos_a_escribir, tamanio);
    pthread_mutex_unlock(&mutex_memoria);

    log_info(logger, "## Escritura de %u bytes", tamanio);
}

void* leer_de_bloque_memoria(uint32_t dir_fisica, uint32_t tamanio) {
    usleep(delay_memoria * 1000);

    void* buffer_lectura = malloc(tamanio);

    pthread_mutex_lock(&mutex_memoria);
    memcpy(buffer_lectura, ms_globals.memory + dir_fisica, tamanio);
    pthread_mutex_unlock(&mutex_memoria);

    log_info(logger, "## Lectura de %u bytes", tamanio);

    return buffer_lectura; 
}

void* atender_cliente(void* arg) {
    int* socket_ptr = (int*)arg;
    int socket_cliente = *socket_ptr;
    free(socket_ptr);

    while (1) {
        int cop = recibir_op_code(socket_cliente);
        
        if (cop == -1) {
            break;
        }

        if (cop == NUEVA_CPU) {
            enviar_op_code(OK, socket_cliente);

        enviar_uint32(base_memory_stick, socket_cliente);
        enviar_uint32(tamanio_memory_stick, socket_cliente);

        log_info(logger,
                "CPU conectada. Base=%u Tam=%u",
                base_memory_stick,
                tamanio_memory_stick);
                
            pthread_mutex_lock(&mutex_memoria);
            list_add(ms_globals.cpus_conectadas, (void*)(intptr_t)socket_cliente);
            pthread_mutex_unlock(&mutex_memoria);

            continue;
        }

        t_list* parametros = recibir_paquete(socket_cliente);

        if (cop == LEER_MEMORIA) {
            uint32_t dir_fisica = *(uint32_t*)list_get(parametros, 0);
            uint32_t tamanio    = *(uint32_t*)list_get(parametros, 1);

            void* bytes_leidos = leer_de_bloque_memoria(dir_fisica, tamanio);

            send(socket_cliente, bytes_leidos, tamanio, 0);

            free(bytes_leidos);
        } 
        else if (cop == ESCRIBIR_MEMORIA) {
            uint32_t dir_fisica  = *(uint32_t*)list_get(parametros, 0);
            uint32_t tamanio     = *(uint32_t*)list_get(parametros, 1);
            void* datos_escribir = list_get(parametros, 2);

            escribir_en_bloque_memoria(dir_fisica, datos_escribir, tamanio);

            enviar_op_code(OK_ESCRITURA, socket_cliente);
        }

        list_destroy_and_destroy_elements(parametros, free);
    }

    close(socket_cliente);
    return NULL;
}
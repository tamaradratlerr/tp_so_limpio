#include "utils.h"

bool mock = false;
t_log* logger;
t_config* config;
t_memory_stick_global ms_globals;
int delay_memoria;
pthread_mutex_t mutex_memoria;

extern void arrancar_cliente_km(void);
extern int socket_km;

int main(int argc, char** argv)
{
    validar_argumentos(argc, argv);

    config = config_create(argv[1]);
    
    if(config == NULL){
        printf("No se pudo abrir el archivo de configuración: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    
    char* file = config_get_string_value(config, "SERVER_LOG_NAME");
    char* process_name = config_get_string_value(config, "PROCESS_NAME");
    char* server_port = config_get_string_value(config, "MEMORY_STICK_PORT");


    delay_memoria = config_get_int_value(config, "MEMORY_DELAY");


    // El tamaño viene por argumento
    uint32_t tamanio_ms = (uint32_t)atoi(argv[2]);

    printf("file: %s\n", file);
    printf("process_name: %s\n", process_name);

    logger = log_create(
        file,
        process_name,
        true,
        log_level_from_string(
            config_get_string_value(config, "LOG_LEVEL")
        )
    );


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
    memcpy(ms_globals.memoria + dir_fisica, datos_a_escribir, tamanio);
    pthread_mutex_unlock(&mutex_memoria);

    log_info(logger,
        "## Escritura de %u bytes en direccion %u. Valor: %d",
        tamanio,
        dir_fisica,
        *(uint8_t*)datos_a_escribir
    );
}

void* leer_de_bloque_memoria(uint32_t dir_fisica, uint32_t tamanio) {
    log_debug(logger, "llegó a leer bloque de memoria");
    
    usleep(delay_memoria * 1000);

    void* buffer_lectura = malloc(tamanio);

    pthread_mutex_lock(&mutex_memoria);

    log_debug(logger,
    "base=%u dir_global=%u tam=%u memoria=%p",
    ms_globals.base,
    dir_fisica,
    tamanio,
    ms_globals.memoria);

    memcpy(buffer_lectura, ms_globals.memoria + dir_fisica, tamanio);
    pthread_mutex_unlock(&mutex_memoria);

    log_debug(logger, "## Lectura de %u bytes", tamanio);
    log_debug(logger, "## Datos leídos: %.*s", (int)tamanio, (char*)buffer_lectura);

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

        // Confirmamos handshake
        enviar_op_code(OK, socket_cliente);

        log_info(logger,
            "Enviando info a CPU: base=%u tamaño=%u",
            ms_globals.base,
            ms_globals.tamanio
        );

        send(socket_cliente,
            &ms_globals.base,
            sizeof(uint32_t),
            0);

        send(socket_cliente,
            &ms_globals.tamanio,
            sizeof(uint32_t),
            0);


        pthread_mutex_lock(&mutex_memoria);

        list_add(ms_globals.cpus_conectadas,
                (void*)(intptr_t)socket_cliente);

        pthread_mutex_unlock(&mutex_memoria);

        continue;
    }
    if (cop == LEER_MEMORIA) {

        uint32_t dir_fisica;
        uint32_t tamanio;

        recv(socket_cliente,
            &dir_fisica,
            sizeof(uint32_t),
            MSG_WAITALL);

        recv(socket_cliente,
            &tamanio,
            sizeof(uint32_t),
            MSG_WAITALL);

        uint32_t dir_local = dir_fisica - ms_globals.base;

        void* bytes = leer_de_bloque_memoria(
            dir_local,
            tamanio
        );

        send(socket_cliente,
            bytes,
            tamanio,
            0);

        free(bytes);
    }
    else if (cop == ESCRIBIR_MEMORIA) {

        uint32_t dir_fisica = recibir_int(socket_cliente);
        uint32_t tamanio = recibir_int(socket_cliente);

        void* datos_escribir = malloc(tamanio);

        if (recv(socket_cliente,
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

        enviar_op_code(OK, socket_cliente);
    }
    }

    close(socket_cliente);
    return NULL;
}
#include "utils.h"

extern t_log* logger;
extern t_config* config;
extern t_memory_stick_globals ms_globals;

void arrancar_cliente_km(void) {
    char* ip_km = config_get_string_value(config, "IP_KM");
    char* puerto_km = config_get_string_value(config, "PUERTO_KM");

    int socket_km = crear_conexion(ip_km, puerto_km, logger, 0);
    if (socket_km < 0) {
        log_error(logger, "Error: No se pudo conectar a la Kernel Memory. Abortando.");
        exit(EXIT_FAILURE);
    }
    log_info(logger, "## Conectado a Kernel Memory");

    t_paquete* p_handshake = crear_paquete(NUEVA_MEMORIA_ACUM); 
    agregar_a_paquete(p_handshake, &(ms_globals.memory_size), sizeof(uint32_t));
    enviar_paquete(p_handshake, socket_km);
    eliminar_paquete(p_handshake);

    pthread_t thread_km;
    int* socket_km_ptr = malloc(sizeof(int));
    *socket_km_ptr = socket_km;
    
    pthread_create(&thread_km, NULL, atender_cliente, socket_km_ptr);
    pthread_detach(thread_km);
    }
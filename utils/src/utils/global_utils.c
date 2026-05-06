#include "global_utils.h"

int crear_conexion(char *ip, char* puerto, t_log* logger, module_name moduleName)
{
	int err;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* 1. Obtener info de dirección */
	err = getaddrinfo(ip, puerto, &hints, &server_info); /*if 0 => ok*/
    if(err) {
        perror("Error on getaddrinfo.");
        abort();
    }

    /* 2. Crear el socket */
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if(socket_cliente == -1) {
        freeaddrinfo(server_info);
		perror("Error: socket closed.");
        abort();
    }

    /* 3. Intentar conectar al socket */
	err = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);
    if(err) {
		perror("Error on connect.");
		close(socket_cliente);
		freeaddrinfo(server_info);
		abort();
    }
    
    freeaddrinfo(server_info);

    /* Logger obligatorio para conexión a servidor */
    log_info(logger, "## Conectado a %s", module_name);

    return socket_cliente;
}

const char* getModuleName(module_name module) {
    switch (module) {
        case KERNEL_SCHEDULER: return "Kernel Scheduler";
        case KERNEL_MEMORY:    return "Kernel Memory";
        case CPU:              return "Cpu";
        case MEMORY_STICK:     return "Memory Stick";
        case SWAP:             return "Swap";
        case IO:               return "Io";
        default:               return "UNKNOWN_MODULE";
    }
}
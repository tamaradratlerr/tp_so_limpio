
#include "./global_utils.h"

// Al principio de global_utils.c, debajo de los #include
extern char *PUERTO; // O como sea que definas el puerto en tu config

t_config *iniciar_config(char *path_config)
{
    t_config *nuevo_config;

    nuevo_config = config_create(path_config);

    if (nuevo_config == NULL)
    {
        printf("No se pudo abrir el config\n");
        abort();
    }

    return nuevo_config;
}

/*-----     MANEJO DE PAQUETES     -----*/

t_paquete *crear_paquete(op_code codigo)
{
    t_paquete *paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = codigo;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->stream = NULL;
    return paquete;
}

void enviar_paquete(t_paquete *paquete, int socket_cliente)
{
    int bytes = paquete->buffer->size + sizeof(op_code) + sizeof(uint32_t);
    void *a_enviar = serializar_paquete(paquete, bytes);

    send(socket_cliente, a_enviar, bytes, 0);

    free(a_enviar);
}

void eliminar_paquete(t_paquete *paquete)
{
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void *serializar_paquete(t_paquete *paquete, int bytes)
{
    void *magic = malloc(bytes);
    int desplazamiento = 0;

    memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(op_code));
    desplazamiento += sizeof(op_code);

    memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);

    return magic;
}

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
    // 1. Redimensionamos el stream para que entre el nuevo dato + su tamaño
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

    // 2. Copiamos el tamaño del dato (payload size del dato específico)
    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));

    // 3. Copiamos el dato en sí justo después del tamaño
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

    // 4. Actualizamos el tamaño total del buffer
    paquete->buffer->size += tamanio + sizeof(int);
}

/*-----     MANEJO DE CONEXIONES     -----*/

int crear_conexion(char *ip, char *puerto, t_log *logger, module_name module)
{
    int err;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* 1. Obtener info de dirección */
    err = getaddrinfo(ip, puerto, &hints, &server_info); /*if 0 => ok*/
    if (err)
    {
        perror("Error on getaddrinfo.");
        abort();
    }

    /* 2. Crear el socket */
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_cliente == -1)
    {
        freeaddrinfo(server_info);
        perror("Error: socket closed.");
        abort();
    }

    /* 3. Intentar conectar al socket */
    err = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);
    if (err)
    {
        perror("Error on connect.");
        close(socket_cliente);
        freeaddrinfo(server_info);
        abort();
    }

    freeaddrinfo(server_info);

    /* Logger obligatorio para conexión a servidor */
    log_info(logger, "## Conectado a %s", getModuleName(module));

    return socket_cliente;
}

int crear_conexion_reintentando(char *ip, char* puerto, t_log* logger, module_name module)
{
    int err;
    struct addrinfo hints, *server_info;

    while (1)
    {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        err = getaddrinfo(ip, puerto, &hints, &server_info);

        if(err){
            log_warning(logger, "Error en getaddrinfo. Reintentando...");
            sleep(5);
            continue;
        }

        int socket_cliente = socket(
            server_info->ai_family,
            server_info->ai_socktype,
            server_info->ai_protocol
        );

        if(socket_cliente == -1){
            freeaddrinfo(server_info);
            log_warning(logger, "Error creando socket. Reintentando...");
            sleep(5);
            continue;
        }

        err = connect(
            socket_cliente,
            server_info->ai_addr,
            server_info->ai_addrlen
        );

        if(err){
            log_warning(logger,
                        "%s no disponible. Reintentando en 5 segundos...",
                        getModuleName(module));

            close(socket_cliente);
            freeaddrinfo(server_info);
            sleep(5);
            continue;
        }

        freeaddrinfo(server_info);

        log_info(logger, "## Conectado a %s", getModuleName(module));

        return socket_cliente;
    }
}

const char *getModuleName(module_name module)
{ // Trabaja adentro de Crear Conexion
    switch (module)
    {
    case KERNEL_SCHEDULER:
        return "Kernel Scheduler";
    case KERNEL_MEMORY:
        return "Kernel Memory";
    case CPU:
        return "Cpu";
    case MEMORY_STICK:
        return "Memory Stick";
    case SWAP:
        return "Swap";
    case IO:
        return "Io";
    default:
        return "UNKNOWN_MODULE";
    }
}

void liberar_conexion(int socket_cliente)
{
    close(socket_cliente);
}

int esperar_cliente(int socket_servidor, t_log *logger)
{
    // Aceptamos un nuevo cliente
    int socket_cliente = accept(socket_servidor, NULL, NULL);

    if (socket_cliente == -1)
    {
        log_info(logger, "Error al aceptar al cliente");
        return -1;
    }

    log_info(logger, "Se conecto un Cliente!");

    return socket_cliente;
}

int iniciar_servidor(char *puerto, t_log *logger)
{

    int socket_servidor;

    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);

    // Creamos el socket de escucha del servidor
    socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    // Asociamos el socket a un puerto
    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

    // Escuchamos las conexiones entrantes
    listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);
    log_info(logger, "Listo para escuchar Clientes");

    return socket_servidor;
}

/*-----     RECEPCION DE INFORMACION     -----*/

void *recibir_buffer(int *size, int socket_cliente)
{
    void *buffer;
    if (recv(socket_cliente, size, sizeof(int), MSG_WAITALL) > 0)
    {
        buffer = malloc(*size);
        recv(socket_cliente, buffer, *size, MSG_WAITALL);
        return buffer;
    }
    return NULL;
}

op_code recibir_op_code(int socket_cliente)
{

    op_code cod_op;
    if (recv(socket_cliente, &cod_op, sizeof(op_code), MSG_WAITALL) > 0)
    {
        return cod_op;
    }
    else
    {
        close(socket_cliente);
        return -1;
    }
}

int recibir_pid(int socket_cliente)
{

    int pid;
    recv(socket_cliente, &pid, sizeof(int), MSG_WAITALL);
    return pid;
}

t_list *recibir_paquete(int socket_cliente)
{
    int size;
    int desplazamiento = 0;
    void *buffer;
    t_list *valores = list_create();
    int tamanio;

    buffer = recibir_buffer(&size, socket_cliente);

    while (desplazamiento < size)
    {
        // 1. Leemos el tamaño del próximo dato (casteando a char*)
        memcpy(&tamanio, (char *)buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);

        // 2. Reservamos memoria para el dato
        char *valor = malloc(tamanio);

        // 3. Leemos el dato en sí
        memcpy(valor, (char *)buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;

        // 4. Lo guardamos en la lista
        list_add(valores, valor);
    }

    free(buffer);
    return valores;
}

/*-----     MANEJO DE MENSAJES     -----*/

void enviar_mensaje(char *mensaje, int socket_cliente)
{
    t_paquete *paquete = crear_paquete(MENSAJE);
    // +1 para incluir el '\0'
    agregar_a_paquete(paquete, mensaje, strlen(mensaje) + 1);
    enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);
}

void enviar_op_code(op_code code_op, int socket_cliente)
{
    // Solo enviamos el código, no hace falta crear un paquete complejo si solo es el op_code
    send(socket_cliente, &code_op, sizeof(op_code), 0);
}

char *recibir_mensaje(int socket_cliente, t_log *logger)
{
    int size;
    char *buffer = recibir_buffer(&size, socket_cliente);
    log_info(logger, "Me llego el mensaje %s", buffer);

    return buffer;
} // HAY QUE LIBERAR LA MEMORIA DSP DE LLAMAR A ESTA FUNCION

/*-----     SISTEMA     -----*/

void iterator(char *value, t_log *logger)
{
    log_info(logger, "%s", value);
}

int enviar_pid(int PCB_ID, int socket_cliente)
{

    send(socket_cliente, &PCB_ID, sizeof(int), 0);

    return 1;
}
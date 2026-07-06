
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
        log_debug(logger, "OPCODE: [%s] fue recibido", opcode_to_string(cod_op));
        return cod_op;
    }
    else
    {
        close(socket_cliente);
        return -1;
    }
}

int recibir_int(int socket_cliente)
{

    int cod_op;
    if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
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
    ssize_t bytes_leidos = recv(socket_cliente, &pid, sizeof(int), MSG_WAITALL);

    if (bytes_leidos <= 0)
    {
        log_error(logger, "Error al recibir PID: la conexion se cerro o hubo un error de socket");
        return -1;
    }

    log_debug(logger, "PID: [%d] fue recibido", pid);
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
    int size = strlen(mensaje) + 1;   // +1 para incluir el '\0'
    enviar_buffer(mensaje, size, socket_cliente);
}

void enviar_buffer(void *buffer, int size, int socket_cliente)
{
    send(socket_cliente, &size, sizeof(int), MSG_WAITALL);
    send(socket_cliente, buffer, size, MSG_WAITALL);
}

void enviar_op_code(op_code code_op, int socket_cliente)
{
    
    log_debug(logger, "Se envia el OP_CODE [%s]",opcode_to_string(code_op));
    // Solo enviamos el código, no hace falta crear un paquete complejo si solo es el op_code
    send(socket_cliente, &code_op, sizeof(op_code), 0);
}

void enviar_int(int code_op, int socket_cliente)
{
    // Solo enviamos el código, no hace falta crear un paquete complejo si solo es el op_code
    send(socket_cliente, &code_op, sizeof(int), 0);
}

char *recibir_mensaje(int socket_cliente, t_log *logger)
{
    int size;
    char *buffer = recibir_buffer(&size, socket_cliente);

    if (buffer == NULL) {
        log_warning(logger, "Se perdió la conexión al intentar recibir un mensaje");
        return NULL;
    }

    log_debug(logger, "Mensaje [%s] Recibido", buffer);
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
    log_debug(logger, "PID: [%d] fue enviado",PCB_ID);

    return 1;
}

#include <commons/log.h>

void log_opcode(t_log* logger, op_code codigo)
{
    switch (codigo)
    {
        case OK:
            log_info(logger, "OP_CODE: OK");
            break;
        case NOTOK:
            log_info(logger, "OP_CODE: NOTOK");
            break;
        case MENSAJE:
            log_info(logger, "OP_CODE: MENSAJE");
            break;
        case PAQUETE:
            log_info(logger, "OP_CODE: PAQUETE");
            break;
        case NUEVA_CPU:
            log_info(logger, "OP_CODE: NUEVA_CPU");
            break;
        case CPU_LIBRE:
            log_info(logger, "OP_CODE: CPU_LIBRE");
            break;
        case NUEVA_MEMORY_STICK:
            log_info(logger, "OP_CODE: NUEVA_MEMORY_STICK");
            break;
        case DESCONEXION_MS:
            log_info(logger, "OP_CODE: DESCONEXION_MS");
            break;
        case FIN_PROCESO:
            log_info(logger, "OP_CODE: FIN_PROCESO");
            break;
        case DESALOJO:
            log_info(logger, "OP_CODE: DESALOJO");
            break;
        case PCB_DATA:
            log_info(logger, "OP_CODE: PCB_DATA");
            break;
        case TRADUCIR_DIRECCION:
            log_info(logger, "OP_CODE: TRADUCIR_DIRECCION");
            break;
        case ERROR_MMU:
            log_info(logger, "OP_CODE: ERROR_MMU");
            break;
        case ERROR_SEGMENTATION_FAULT:
            log_info(logger, "OP_CODE: ERROR_SEGMENTATION_FAULT");
            break;
        case gl_MUTEX_CREATE:
            log_info(logger, "OP_CODE: gl_MUTEX_CREATE");
            break;
        case gl_MUTEX_LOCK:
            log_info(logger, "OP_CODE: gl_MUTEX_LOCK");
            break;
        case gl_MUTEX_UNLOCK:
            log_info(logger, "OP_CODE: gl_MUTEX_UNLOCK");
            break;
        case gl_MEM_ALLOC:
            log_info(logger, "OP_CODE: gl_MEM_ALLOC");
            break;
        case gl_MEM_FREE:
            log_info(logger, "OP_CODE: gl_MEM_FREE");
            break;
        case gl_IO_SLEEP:
            log_info(logger, "OP_CODE: gl_IO_SLEEP");
            break;
        case gl_IO_STDOUT:
            log_info(logger, "OP_CODE: gl_IO_STDOUT");
            break;
        case gl_IO_STDIN:
            log_info(logger, "OP_CODE: gl_IO_STDIN");
            break;
        case gl_INIT_PROC:
            log_info(logger, "OP_CODE: gl_INIT_PROC");
            break;
        case gl_EXIT:
            log_info(logger, "OP_CODE: gl_EXIT");
            break;
        case FETCH_INSTRUCCION:
            log_info(logger, "OP_CODE: FETCH_INSTRUCCION");
            break;
        case LLEGO_INSTRUCCION:
            log_info(logger, "OP_CODE: LLEGO_INSTRUCCION");
            break;
        case FETCH:
            log_info(logger, "OP_CODE: FETCH");
            break;
        case LEER_MEMORIA:
            log_info(logger, "OP_CODE: LEER_MEMORIA");
            break;
        case ESCRIBIR_MEMORIA:
            log_info(logger, "OP_CODE: ESCRIBIR_MEMORIA");
            break;
        case CONTEXTO:
            log_info(logger, "OP_CODE: CONTEXTO");
            break;
        case MEM_CORRUPT:
            log_info(logger, "OP_CODE: MEM_CORRUPT");
            break;
        case ENVIAR_PROCESO:
            log_info(logger, "OP_CODE: ENVIAR_PROCESO");
            break;
        case km_IO_STDOUT:
            log_info(logger, "OP_CODE: km_IO_STDOUT");
            break;
        case km_IO_STDIN:
            log_info(logger, "OP_CODE: km_IO_STDIN");
            break;
        case SOLICITUD_INSTRUCCION:
            log_info(logger, "OP_CODE: SOLICITUD_INSTRUCCION");
            break;
        case km_GUARDAR_CONTEXTO:
            log_info(logger, "OP_CODE: km_GUARDAR_CONTEXTO");
            break;
        case ks_INIT_PROC:
            log_info(logger, "OP_CODE: ks_INIT_PROC");
            break;
        case ks_EXIT:
            log_info(logger, "OP_CODE: ks_EXIT");
            break;
        case NUEVO_KERNEL:
            log_info(logger, "OP_CODE: NUEVO_KERNEL");
            break;
        case OK_ESCRITURA:
            log_info(logger, "OP_CODE: OK_ESCRITURA");
            break;
        case NUEVO_ESPACIO:
            log_info(logger, "OP_CODE: NUEVO_ESPACIO");
            break;
        case SUSPENDIDO:
            log_info(logger, "OP_CODE: SUSPENDIDO");
            break;
        case NUEVA_IO:
            log_info(logger, "OP_CODE: NUEVA_IO");
            break;
        case IO_LIBRE:
            log_info(logger, "OP_CODE: IO_LIBRE");
            break;
        case IO_STDIN:
            log_info(logger, "OP_CODE: IO_STDIN");
            break;
        case IO_STDOUT:
            log_info(logger, "OP_CODE: IO_STDOUT");
            break;
        case IO_SLEEP:
            log_info(logger, "OP_CODE: IO_SLEEP");
            break;
        case IO_STDOUT_RETORNO:
            log_info(logger, "OP_CODE: IO_STDOUT_RETORNO");
            break;
        case IO_STDIN_RETORNO:
            log_info(logger, "OP_CODE: IO_STDIN_RETORNO");
            break;
        case CPUS_DESALOJADAS_OK:
            log_info(logger, "OP_CODE: CPUS_DESALOJADAS_OK");
            break;
        case COMPACTACION:
            log_info(logger, "OP_CODE: COMPACTACION");
            break;
        case COMPACTACION_FINALIZADA:
            log_info(logger, "OP_CODE: COMPACTACION_FINALIZADA");
            break;
        case NUEVA_MEMORIA_ACUM:
            log_info(logger, "OP_CODE: NUEVA_MEMORIA_ACUM");
            break;

        default:
            log_info(logger, "OP_CODE: DESCONOCIDO (%d)", codigo);
            break;
    }
}

char* opcode_to_string(op_code codigo)
{
    switch (codigo)
    {
        case OK: return "OP_CODE: OK";
        case NOTOK: return "OP_CODE: NOTOK";
        case MENSAJE: return "OP_CODE: MENSAJE";
        case PAQUETE: return "OP_CODE: PAQUETE";
        case NUEVA_CPU: return "OP_CODE: NUEVA_CPU";
        case CPU_LIBRE: return "OP_CODE: CPU_LIBRE";
        case NUEVA_MEMORY_STICK: return "OP_CODE: NUEVA_MEMORY_STICK";
        case DESCONEXION_MS: return "OP_CODE: DESCONEXION_MS";
        case FIN_PROCESO: return "OP_CODE: FIN_PROCESO";
        case DESALOJO: return "OP_CODE: DESALOJO";
        case PCB_DATA: return "OP_CODE: PCB_DATA";
        case TRADUCIR_DIRECCION: return "OP_CODE: TRADUCIR_DIRECCION";
        case ERROR_MMU: return "OP_CODE: ERROR_MMU";
        case ERROR_SEGMENTATION_FAULT: return "OP_CODE: ERROR_SEGMENTATION_FAULT";
        case gl_MUTEX_CREATE: return "OP_CODE: gl_MUTEX_CREATE";
        case gl_MUTEX_LOCK: return "OP_CODE: gl_MUTEX_LOCK";
        case gl_MUTEX_UNLOCK: return "OP_CODE: gl_MUTEX_UNLOCK";
        case gl_MEM_ALLOC: return "OP_CODE: gl_MEM_ALLOC";
        case gl_MEM_FREE: return "OP_CODE: gl_MEM_FREE";
        case gl_IO_SLEEP: return "OP_CODE: gl_IO_SLEEP";
        case gl_IO_STDOUT: return "OP_CODE: gl_IO_STDOUT";
        case gl_IO_STDIN: return "OP_CODE: gl_IO_STDIN";
        case gl_INIT_PROC: return "OP_CODE: gl_INIT_PROC";
        case gl_EXIT: return "OP_CODE: gl_EXIT";
        case FETCH_INSTRUCCION: return "OP_CODE: FETCH_INSTRUCCION";
        case LLEGO_INSTRUCCION: return "OP_CODE: LLEGO_INSTRUCCION";
        case FETCH: return "OP_CODE: FETCH";
        case LEER_MEMORIA: return "OP_CODE: LEER_MEMORIA";
        case ESCRIBIR_MEMORIA: return "OP_CODE: ESCRIBIR_MEMORIA";
        case CONTEXTO: return "OP_CODE: CONTEXTO";
        case MEM_CORRUPT: return "OP_CODE: MEM_CORRUPT";
        case ENVIAR_PROCESO: return "OP_CODE: ENVIAR_PROCESO";
        case km_IO_STDOUT: return "OP_CODE: km_IO_STDOUT";
        case km_IO_STDIN: return "OP_CODE: km_IO_STDIN";
        case SOLICITUD_INSTRUCCION: return "OP_CODE: SOLICITUD_INSTRUCCION";
        case km_GUARDAR_CONTEXTO: return "OP_CODE: km_GUARDAR_CONTEXTO";
        case ks_INIT_PROC: return "OP_CODE: ks_INIT_PROC";
        case ks_EXIT: return "OP_CODE: ks_EXIT";
        case NUEVO_KERNEL: return "OP_CODE: NUEVO_KERNEL";
        case OK_ESCRITURA: return "OP_CODE: OK_ESCRITURA";
        case NUEVO_ESPACIO: return "OP_CODE: NUEVO_ESPACIO";
        case SUSPENDIDO: return "OP_CODE: SUSPENDIDO";
        case NUEVA_IO: return "OP_CODE: NUEVA_IO";
        case IO_LIBRE: return "OP_CODE: IO_LIBRE";
        case IO_STDIN: return "OP_CODE: IO_STDIN";
        case IO_STDOUT: return "OP_CODE: IO_STDOUT";
        case IO_SLEEP: return "OP_CODE: IO_SLEEP";
        case IO_STDOUT_RETORNO: return "OP_CODE: IO_STDOUT_RETORNO";
        case IO_STDIN_RETORNO: return "OP_CODE: IO_STDIN_RETORNO";
        case CPUS_DESALOJADAS_OK: return "OP_CODE: CPUS_DESALOJADAS_OK";
        case COMPACTACION: return "OP_CODE: COMPACTACION";
        case COMPACTACION_FINALIZADA: return "OP_CODE: COMPACTACION_FINALIZADA";
        case NUEVA_MEMORIA_ACUM: return "OP_CODE: NUEVA_MEMORIA_ACUM";

        default:
            return "OP_CODE: DESCONOCIDO";
    }
}
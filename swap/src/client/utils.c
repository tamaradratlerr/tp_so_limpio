#define _XOPEN_SOURCE 700 //para asegurar que se reconozca pread y pwrite

#include "utils.h"

static char* archivo_swap_path = NULL;
static int tamanio_total_swap = 0;
static int tamanio_bloque_swap = 0;
static pthread_mutex_t mutex_swap = PTHREAD_MUTEX_INITIALIZER;

// CHEQUEAR ESTO QUE SI ESTA EN GLOBAL UTILS NO HACE FALTA
void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
} 

int crear_conexion(char *ip, char* puerto)
{
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // 1. Obtener info de dirección
    if(getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        fprintf(stderr, "Error en getaddrinfo\n");
        return -1;
    }

    // 2. Crear el socket
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if(socket_cliente == -1) {
        freeaddrinfo(server_info);
        return -1;
    }

    // 3. Intentar conectar
    if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        // Si entra acá, la conexión NO se realizó
        perror("ERROR AL CONECTAR"); 
        close(socket_cliente); // Cerramos el socket porque no sirve para nada
        freeaddrinfo(server_info);
        return -1; // Devolvemos -1 para avisar al main que falló
    }

    freeaddrinfo(server_info);
    return socket_cliente;
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

//------------------------------
//FUNCIONES DE RECEPCION

int recibir_operacion(int socket_cliente)
{
    int cod_op;

    if(recv(socket_cliente,
            &cod_op,
            sizeof(int),
            MSG_WAITALL) > 0)
    {
        return cod_op;
    }

    return -1;
}

void* recibir_buffer(int* size, int socket_cliente) { //sirve para leer paquetes
    void * buffer;
    if(recv(socket_cliente, size, sizeof(int), MSG_WAITALL) > 0) {
        buffer = malloc(*size);
        recv(socket_cliente, buffer, *size, MSG_WAITALL);
        return buffer;
    }
    return NULL;
}

//swap 

void inicializar_swap(char* path, int tamanio_swap, int tamanio_bloque)
{
    archivo_swap_path = path;
    tamanio_total_swap = tamanio_swap;
    tamanio_bloque_swap = tamanio_bloque;

    int fd = open(path, O_CREAT | O_RDWR, 0664);

    if(fd == -1){
        perror("Error al crear archivo swap");
        exit(EXIT_FAILURE);
    }

// consigna: “SWAP contará con un único archivo cuyo path y tamaño serán definidos por config”.
    if(ftruncate(fd, tamanio_swap) == -1){  //crea el archivo con el tamaño exacto que diga config
        perror("Error al dimensionar archivo swap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

//Sirve para recibir un paquete armado con agregar_a_paquete

t_list* recibir_paquete(int socket_cliente)
{
    int size;
    int desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket_cliente);
    t_list* valores = list_create();

    while(desplazamiento < size)
    {
        int tamanio;

        memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);

        void* valor = malloc(tamanio);

        memcpy(valor, buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;

        list_add(valores, valor);
    }

    free(buffer);
    return valores;
}

//armo una confirmacion de escritura, dsp de escribir swap avisa que todo salio bien.
void enviar_respuesta_simple(int socket_km, op_code respuesta)
{
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = respuesta;
    enviar_paquete(paquete, socket_km);
    eliminar_paquete(paquete);
}


//consigna: SWAP debe permitir leer bloques desde el archivo de swap.
//recibe un paquete, lo convierte en lista y después lee cada dato por posición.
void manejar_lectura_bloque(int socket_km, t_log* logger)
{
    t_list* paquete = recibir_paquete(socket_km);

    int numero_bloque = *(int*) list_get(paquete, 0);
    int cantidad_bloques = tamanio_total_swap / tamanio_bloque_swap;

    log_info(logger, "## Lectura del bloque: %d", numero_bloque); //para registrar cada vez q la kernel pide leer un bloque

    if(numero_bloque < 0 || numero_bloque >= cantidad_bloques) {
        list_destroy_and_destroy_elements(paquete, free);
        enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
        return;
    }

    void* datos = malloc(tamanio_bloque_swap);
    int offset = numero_bloque * tamanio_bloque_swap;

    pthread_mutex_lock(&mutex_swap);

    int fd = open(archivo_swap_path, O_RDONLY);

    if(fd == -1) {
        pthread_mutex_unlock(&mutex_swap);
        free(datos);
        list_destroy_and_destroy_elements(paquete, free);
        enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
        return;
    }

    ssize_t bytes_leidos = pread(fd, datos, tamanio_bloque_swap, offset);
    close(fd);

    pthread_mutex_unlock(&mutex_swap);

    if(bytes_leidos != tamanio_bloque_swap) {
        free(datos);
        list_destroy_and_destroy_elements(paquete, free);
        enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
        return;
    }

    t_paquete* respuesta = crear_paquete();
    respuesta->codigo_operacion = RESPUESTA_DATOS;

    agregar_a_paquete(respuesta, datos, tamanio_bloque_swap);
    enviar_paquete(respuesta, socket_km);
    eliminar_paquete(respuesta);

    free(datos);
    list_destroy_and_destroy_elements(paquete, free);
}

//recibe numero de bloque y datos a guardar y escribe esos bytes en la posición correcta del archivo.
//consigna: SWAP debe permitir escribir bloques en el archivo de swap.
//recibe numero de bloque y datos a guardar, y los escribe en la posicion correcta del archivo.
void manejar_escritura_bloque(int socket_km, t_log* logger)
{
    t_list* paquete = recibir_paquete(socket_km);

    int numero_bloque = *(int*) list_get(paquete, 0);
    void* datos = list_get(paquete, 1);

    log_info(logger, "## Escritura del bloque: %d", numero_bloque);

    int tamanio_datos = tamanio_bloque_swap;
    int cantidad_bloques = tamanio_total_swap / tamanio_bloque_swap;

    if(numero_bloque < 0 || numero_bloque >= cantidad_bloques) {
        list_destroy_and_destroy_elements(paquete, free);
        enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
        return;
    }

    int offset = numero_bloque * tamanio_bloque_swap;

    pthread_mutex_lock(&mutex_swap);

    int fd = open(archivo_swap_path, O_RDWR);

    if(fd == -1) {
        pthread_mutex_unlock(&mutex_swap);
        list_destroy_and_destroy_elements(paquete, free);
        enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
        return;
    }

    ssize_t bytes_escritos = pwrite(fd, datos, tamanio_datos, offset);
    close(fd);

    pthread_mutex_unlock(&mutex_swap);

    if(bytes_escritos != tamanio_datos) {
        list_destroy_and_destroy_elements(paquete, free);
        enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
        return;
    }

    enviar_respuesta_simple(socket_km, RESPUESTA_OK);

    list_destroy_and_destroy_elements(paquete, free);
}


//KERNEL MEMORY
//swap se encuentra esperando recibir pedidos, si kernel se desconecta, debe cortar.
void atender_kernel(int socket_km, t_log* logger)
{
    while(1)
    {
        //op_code cod_op = recibir_operacion(socket_km);
        int cod_op = recibir_operacion(socket_km);

        switch(cod_op)
        {
           case LECTURA_BLOQUE:
                 manejar_lectura_bloque(socket_km, logger);
                 break;

            case ESCRITURA_BLOQUE:
                 manejar_escritura_bloque(socket_km, logger);
                 break;

            case -1:
                close(socket_km);
                return;

            default:
                enviar_respuesta_simple(socket_km, RESPUESTA_ERROR);
                break;
        }
    }
}
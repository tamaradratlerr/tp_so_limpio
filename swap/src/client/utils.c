#define _XOPEN_SOURCE 700 //para asegurar que se reconozca pread y pwrite

#include "utils.h"

static char* archivo_swap_path = NULL;
static int tamanio_total_swap = 0;
static int tamanio_bloque_swap = 0;
static pthread_mutex_t mutex_swap = PTHREAD_MUTEX_INITIALIZER;


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
    send(socket_km, &respuesta, sizeof(op_code), 0);
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
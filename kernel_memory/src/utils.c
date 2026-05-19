#define _POSIX_C_SOURCE 200112L 
#include "utils.h"              
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>


int iniciar_servidor(char* puerto) {
    int socket_servidor;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);
    socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);
    return socket_servidor;
}

int esperar_cliente(int socket_servidor) {
    return accept(socket_servidor, NULL, NULL);
}

int recibir_operacion(int socket_cliente) {
    int cod_op;
    if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
        return cod_op;
    close(socket_cliente);
    return -1;
}

void* recibir_buffer(int* size, int socket_cliente) {
    void* buffer;
    if(recv(socket_cliente, size, sizeof(int), MSG_WAITALL) > 0) {
        buffer = malloc(*size);
        recv(socket_cliente, buffer, *size, MSG_WAITALL);
        return buffer;
    }
    return NULL;
}

void recibir_mensaje(int socket_cliente) {

    int size;
    char* buffer = recibir_buffer(&size, socket_cliente);
    log_info(logger, "Socket %d dice: %s", socket_cliente, buffer);
    free(buffer);
}

t_list* recibir_paquete(int socket_cliente) {
    int size;
    int desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket_cliente);
    t_list* valores = list_create();

    while(desplazamiento < size) {
        int tamanio;
        memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);
        char* valor = malloc(tamanio);
        memcpy(valor, buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;
        list_add(valores, valor);
    }
    free(buffer);
    return valores;
}

//“almacenar un contexto de ejecución por PID”

//registros
typedef struct {
    int ax, bx, cx, dx;
    int pc; //program counter
} t_registros;

//segmentos
typedef struct {
    int id;
    int base;
    int limite;
} t_segmento;

//contexto de ejecucion
typedef struct {
    int pid;
    t_registros registros; //una copia de todos los registros de la CPU
    t_list* tabla_segmentos; //tabla de segmentos asociada a dicho Proceso.
} t_contexto;

//“Este módulo deberá mantener el contexto de ejecución de cada Proceso del sistema”
//-> tengo que guardar TODOS los contextos de TODOS los procesos
// global pq la usan varias partes del programa:crear proceso, cpu, finalizar proceso.

//inicializacion que va en el main EN UTILS.C
lista_contextos = list_create();
pthread_mutex_init(&mutex_contextos, NULL);

//EN UTILS.H
t_list* lista_contextos;
pthread_mutex_t mutex_contextos;


t_contexto* crear_contexto(int pid) {

    t_contexto* ctx = malloc(sizeof(t_contexto));

    ctx->pid = pid;

    //inicializar todos los registros asociados con el valor 0
    ctx->registros.ax = 0;
    ctx->registros.bx = 0;
    ctx->registros.cx = 0;
    ctx->registros.dx = 0;
    ctx->registros.pc = 0;

    ctx->tabla_segmentos = list_create();

    return ctx;
}
//MULTIHILO
void agregar_contexto(int pid) {
pthread_mutex_lock(&mutex_contextos); //Evitar que 2 hilos toquen la lista al mismo tiempo.

t_contexto* ctx = crear_contexto(pid); //“crear el contexto de ejecución del Proceso”
list_add(lista_contextos, ctx); //lo guardo en mi lista

pthread_mutex_unlock(&mutex_contextos); //libero el acceso a la lista.
}

//en utils.h
typedef struct {
    int pid;
    t_list* instrucciones;
} t_proceso;
//---------

//recibir pid y path
//pid identificar el proceso y path ¨direccion¨ para abrir el archivo
void manejar_crear_proceso(int socket_cliente) {

    t_list* paquete = recibir_paquete(socket_cliente);

    int pid = atoi(list_get(paquete, 0));
    char* path = list_get(paquete, 1);


//“Abrir archivo con fopen”
//abrir el archivo donde están las instrucciones del proceso para leer instrucciones
//y poder crear el proceso

FILE* archivo = fopen(path, "r"); //abro el archivo

//validar error
if (archivo == NULL) {
    log_error(logger, "No se pudo abrir el archivo: %s", path);
    return;
}

//“El Kernel Memory deberá ser capaz de enviar la instrucción correspondiente a cada pedido de la CPU”
//“leer con fgets y limpiar con string_trim”

t_list* instrucciones = list_create(); //donde se van a guardar las instrucciones

char linea[256];  //tam de buffer elegido por mi

while (fgets(linea, sizeof(linea), archivo) != NULL) { //lee una linea, la guarda y termina cuando ya no hay más

    char* instruccion = string_trim(linea); // saca /n los cuales se nombran en la consigna

    list_add(instrucciones, strdup(instruccion)); //guardo la instruccion en la lista
}

fclose(archivo);

//“Por cada PID del sistema se tendrá un archivo de pseudocódigo…
//se deberá implementar una estructura que le permita asociar qué PID tiene asociado qué archivo”

//guardar proceso (PID + instrucciones)
   
t_proceso* proceso = malloc(sizeof(t_proceso));
proceso->pid = pid;
proceso->instrucciones = instrucciones;

//guardo el programa del proceso en memoria ya que no puedo estar constantemente abriendo el archivo
pthread_mutex_lock(&mutex_procesos);
list_add(lista_procesos, proceso);
pthread_mutex_unlock(&mutex_procesos);

//crear el contexto de ejecución del Proceso
agregar_contexto(pid);

//log obligatorio
log_info(logger, "## PID: %d - Proceso Creado", pid);

//liberar paquete
list_destroy_and_destroy_elements(paquete, free);

}

//FINALIZACION DE PROCESO
//a partir de este PID recibido se deberán liberar todos los segmentos asociados al Proceso 
// y todas las estructuras asociadas al mismo.

int buscar_indice_proceso(int pid) {

    for(int i = 0; i < list_size(lista_procesos); i++) { //recorro la lista de procesos

        t_proceso* proceso = list_get(lista_procesos, i); //obtengo un proceso de la lista

        if(proceso->pid == pid) { //si coincide el pid
            return i; //devuelvo la posicion
        }
    }

    return -1; //si no existe
}


//debo tmb liberar contexto pq la consigna dice todas las estructuras asociadas al mismo
//reutilizo la logica anterior

int buscar_indice_contexto(int pid) {

    for(int i = 0; i < list_size(lista_contextos); i++) {

        t_contexto* contexto = list_get(lista_contextos, i);

        if(contexto->pid == pid) {
            return i;
        }
    }

    return -1;
}


void manejar_finalizar_proceso(int socket_cliente) {
    
    

    t_list* paquete = recibir_paquete(socket_cliente); //recibe el pid

    int pid = atoi(list_get(paquete, 0)); //obtiene el pid del paquete (atoi= convierte texto a número)

    //ahora identifico que proceso debe eliminar

    int indice = buscar_indice_proceso(pid); //utilizo la funcion con el pid que obtuve del paquete

    if(indice == -1) {

        log_error(logger, "No se encontró el proceso PID: %d", pid);

        list_destroy_and_destroy_elements(paquete, free); //recibir paquete usa memoria dinamica, libero el paquete pq no me sirve
        //evito memory leak

        return;
    }

    //caso en que si lo encontre “removerlo de la lista y liberar su memoria”
    pthread_mutex_lock(&mutex_procesos); //no permito que otros hilos entren a la lista de procesos

    t_proceso* proceso = list_remove(lista_procesos, indice); //devuelve el proceso removido, es de bib de commons

    pthread_mutex_unlock(&mutex_procesos);

    //libero todas las instrucciones del proceso pq antes use strdup(instruccion) y usa mem. dinam
    list_destroy_and_destroy_elements(proceso->instrucciones, free);

    //libero la struct proceso
    free(proceso);


    
    //buscar contexto del PID
    int indice_contexto = buscar_indice_contexto(pid);

    if(indice_contexto != -1) { //valido que existe

        pthread_mutex_lock(&mutex_contextos);

        //remover contexto
        t_contexto* contexto = list_remove(lista_contextos, indice_contexto); //sacar contexto de la lista.

        pthread_mutex_unlock(&mutex_contextos);

        //liberar tabla de segmentos
        list_destroy_and_destroy_elements(contexto->tabla_segmentos, free);

        //liberar contexto
        free(contexto);
    }


    //libero el paquete recibido por socket
    list_destroy_and_destroy_elements(paquete, free);
}
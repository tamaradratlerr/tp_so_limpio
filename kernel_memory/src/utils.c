#define _POSIX_C_SOURCE 200112L 
#include "utils.h"              
#include <sys/socket.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

//“almacenar un contexto de ejecución por PID”


//“Este módulo deberá mantener el contexto de ejecución de cada Proceso del sistema”
//-> tengo que guardar TODOS los contextos de TODOS los procesos
// global pq la usan varias partes del programa:crear proceso, cpu, finalizar proceso.

//inicializacion que va en el main EN UTILS.C

t_list* lista_contextos;
pthread_mutex_t mutex_contextos;
t_list* lista_procesos;
pthread_mutex_t mutex_procesos;

void inicializar_utils(void) {
    lista_contextos = list_create();
    pthread_mutex_init(&mutex_contextos, NULL);

    lista_procesos = list_create();
    pthread_mutex_init(&mutex_procesos, NULL);}

t_contexto* crear_contexto(int pid) {

    t_contexto* ctx = malloc(sizeof(t_contexto));

    ctx->pid = pid;

    //inicializar todos los registros asociados con el valor 0
    memset(ctx, 0, sizeof(t_contexto));
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

//recibir pid y path
//pid identificar el proceso y path ¨direccion¨ para abrir el archivo
void manejar_crear_proceso(int socket_cliente) {
    t_list* paquete = recibir_paquete(socket_cliente);

    // 1. Extraemos los datos crudos del paquete
    char* texto_pid = (char*) list_get(paquete, 0);
    char* path_original = (char*) list_get(paquete, 1);

    int pid = atoi(texto_pid);

    // 2. DUPLICAMOS el path inmediatamente en una variable propia del stack/heap local.
    // Esto nos independiza por completo de lo que haga el paquete del socket.
    char* path = strdup(path_original);

    FILE* archivo = fopen(path, "r");
    
    // Si falló el archivo, liberamos de forma segura y salimos
    if (archivo == NULL) {
        log_error(logger, "No se pudo abrir el archivo: %s", path);
        free(path);
        // Cambiamos el destroy_elements por un destroy común por si los elementos no eran mutables
        list_destroy(paquete); 
        return;
    }

    t_list* instrucciones = list_create();
    char linea[256];  
    while (fgets(linea, sizeof(linea), archivo) != NULL) { 
      
        char* instruccion_duplicada = strdup(linea); 
        string_trim(instruccion_duplicada); 
        list_add(instrucciones, instruccion_duplicada);

    fclose(archivo);
    free(path); // Ya no necesitamos la copia local del path

    // 3. Guardamos el proceso en la lista global
    t_proceso* proceso = malloc(sizeof(t_proceso));
    proceso->pid = pid;
    proceso->instrucciones = instrucciones;

    pthread_mutex_lock(&mutex_procesos);
    list_add(lista_procesos, proceso);
    pthread_mutex_unlock(&mutex_procesos);

    // Crear el contexto de ejecución del Proceso
    agregar_contexto(pid);

    log_info(logger, "## PID: %d - Proceso Creado Exitosamente", pid);

    // 4. LA CLAVE DE LA SOLUCIÓN:
    // Si list_destroy_and_destroy_elements(paquete, free) te daba error, 
    // probá usando list_destroy(paquete) a secas. 
    // Si adentro tenés un buffer único, deberías liberar el buffer contenedor (si tenés la referencia),
    // pero destruir la estructura de la lista con list_destroy NO va a tocar los punteros internos y evitará el crash.
    list_destroy(paquete);
}

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
        //se deberia hacer la parte de borrar del memory stick
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

void manejar_pedido_instruccion_cpu(int socket_cliente) {
    t_list* paquete = recibir_paquete(socket_cliente);

    int pid = *(int*)list_get(paquete, 0);
    uint32_t pc = *(uint32_t*)list_get(paquete, 1);

    int indice = buscar_indice_proceso(pid);
    if (indice == -1) {
        log_error(logger, "CPU pidió instrucción para PID: %d inexistente", pid);
        int tam_error = strlen("ERROR") + 1;
        void* error_buffer = malloc(sizeof(int) + tam_error);
        memcpy(error_buffer, &tam_error, sizeof(int));
        memcpy(error_buffer + sizeof(int), "ERROR", tam_error);
        send(socket_cliente, error_buffer, sizeof(int) + tam_error, 0);
        free(error_buffer);
        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    pthread_mutex_lock(&mutex_procesos);
    t_proceso* proceso = list_get(lista_procesos, indice);
    
    // SOLUCIÓN AL ERROR: Le agregamos el casteo (char*) para que el compilador no chille
    char* instruccion = (char*)list_get(proceso->instrucciones, pc);
    pthread_mutex_unlock(&mutex_procesos);

    int tam_instruccion = strlen(instruccion) + 1; 
    int tam_a_enviar = sizeof(int) + tam_instruccion;
    void* buffer_enviar = malloc(tam_a_enviar);

    int desplazamiento = 0;
    memcpy(buffer_enviar + desplazamiento, &tam_instruccion, sizeof(int));
    desplazamiento += sizeof(int);
    memcpy(buffer_enviar + desplazamiento, instruccion, tam_instruccion);
   
    send(socket_cliente, buffer_enviar, tam_a_enviar, 0);
    log_info(logger, "## CPU - PID: %d - Leyendo PC: %d - Instrucción: %s", pid, pc, instruccion);

    free(buffer_enviar);
    list_destroy_and_destroy_elements(paquete, free);
}


//Se conecta con ks: stdout
void manejar_lectura_memoria(int socket_cliente) {
    t_list* paquete = recibir_paquete(socket_cliente);
    uint32_t dir_fisica = *(uint32_t)list_get(paquete, 1);
    uint32_t tamanio = *(uint32_t*)list_get(paquete, 2);

    // MOCK: Devolvemos espacio simulado vacío
    void* datos_falsos = calloc(1, tamanio); 
    send(socket_cliente, datos_falsos, tamanio, 0);

    log_info(logger, "## MOCK LECTURA - Enviados %d bytes en cero a CPU", tamanio);

    free(datos_falsos);
    list_destroy_and_destroy_elements(paquete, free);
}

//Se conecta con ks: stdin
//yo (tami) puse tambien el pid (te mando el pid)
void manejar_escritura_memoria(int socket_cliente) {
    t_list* paquete = recibir_paquete(socket_cliente);
    uint32_t dir_fisica = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio = *(uint32_t*)list_get(paquete, 2);

    // MOCK: Respondemos con confirmación fija (OK = 1)
    int confirmacion = 1; 
    send(socket_cliente, &confirmacion, sizeof(int), 0);

    log_info(logger, "## MOCK ESCRITURA - Confirmado OK a CPU sin impacto fisico");

    list_destroy_and_destroy_elements(paquete, free);
}

//falta poner que me envies lo que tengo que mostrar



//VER NOTION PONER ENVIAR CONTEXTTO Y GUARDAR CONTEXTO
void manejar_guardar_contexto(int socket_cliente) {
    t_list* paquete = recibir_paquete(socket_cliente);
    
    // Lo primero que sacamos siempre es el PID para saber de quién es este contexto
    int pid = *(int*)list_get(paquete, 0);

    int indice = buscar_indice_contexto(pid);
    if (indice != -1) {
        pthread_mutex_lock(&mutex_contextos);
        t_contexto* ctx = list_get(lista_contextos, indice);
        
        // Desempaquetamos absolutamente TODOS los registros en orden
        ctx->pc  = *(uint32_t*)list_get(paquete, 1);
        
        
        ctx->ax  = *(uint8_t*)list_get(paquete, 2);
        ctx->bx  = *(uint8_t*)list_get(paquete, 3);
        ctx->cx  = *(uint8_t*)list_get(paquete, 4);
        ctx->dx  = *(uint8_t*)list_get(paquete, 5);
        
    
        ctx->eax = *(uint32_t*)list_get(paquete, 6);
        ctx->ebx = *(uint32_t*)list_get(paquete, 7);
        ctx->ecx = *(uint32_t*)list_get(paquete, 8);
        ctx->edx = *(uint32_t*)list_get(paquete, 9);
        
       
        ctx->si  = *(uint32_t*)list_get(paquete, 10);
        ctx->di  = *(uint32_t*)list_get(paquete, 11);
        
        pthread_mutex_unlock(&mutex_contextos);
        log_info(logger, "## Contexto Resguardado Completo - PID: %d - PC: %u", pid, ctx->registros.pc);
    } else {
        log_error(logger, "No se encontró el contexto para el PID: %d al intentar resguardar", pid);
    }

    // Liberamos toda la memoria de la lista y sus elementos (incluyendo los registros que ya copiamos)
    list_destroy_and_destroy_elements(paquete, free);
}


















//desalojo: identificar que hqay que hacer compactación y envair opcode a ks
void enviar_desalojo_ks(int socket_ks){
    //identificar compactacion --> ustedes la detectan y en el CP3 con facu la hacemos
        /*
            1. hacer memory stick
            2. guardar procesos
            3. identificar compactcion
        
        */

    enviar_op_code(DESALOJO, socket_ks)
}

void enviar_contexto_cpu(coket_cpu){

}


void recibir_contexto_cpu(coket_cpu){
    
}
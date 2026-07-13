#define _POSIX_C_SOURCE 200809L
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
#include <math.h>
#include "../../utils/src/global_utils.h"


/*Declaracion de Variables*/
pthread_mutex_t mutex_lista_libres;
t_list* lista_huecos_libres;

t_list* lista_contextos;
pthread_mutex_t mutex_contextos;

t_list* lista_procesos;
pthread_mutex_t mutex_procesos;

t_list* lista_memory_sticks;
pthread_mutex_t mutex_ms;
uint32_t memoria_total_sistema = 0;

/*Funciones*/

// La función encierra TODO la creación de estructuras juntas
void inicializar_utils(void)
{
    
    lista_contextos = list_create();
    pthread_mutex_init(&mutex_contextos, NULL);

    lista_procesos = list_create();
    pthread_mutex_init(&mutex_procesos, NULL); 

    lista_memory_sticks = list_create();
    pthread_mutex_init(&mutex_ms, NULL);
    
    lista_huecos_libres = list_create();
    pthread_mutex_init(&mutex_lista_libres, NULL);
} 

t_contexto* crear_contexto(int pid) 
{

    t_contexto* ctx = malloc(sizeof(t_contexto));

    ctx->pid = pid;

    ctx->pc = 0;
    ctx->ax = 0;
    ctx->bx = 0;
    ctx->cx = 0;
    ctx->dx = 0;
    ctx->eax = 0;
    ctx->ebx = 0;
    ctx->ecx = 0;
    ctx->edx = 0;
    ctx->si = 0;
    ctx->di = 0;
    
    ctx->tabla_segmentos = list_create();
    
    return ctx;
}

//MULTIHILO
void agregar_contexto(int pid) 
{
    pthread_mutex_lock(&mutex_contextos); //Evitar que 2 hilos toquen la lista al mismo tiempo.

    t_contexto* ctx = crear_contexto(pid); //“crear el contexto de ejecución del Proceso”
    list_add(lista_contextos, ctx); //lo guardo en mi lista

    pthread_mutex_unlock(&mutex_contextos); //libero el acceso a la lista.
}


void manejar_crear_proceso(int socket_cliente) {
    
    int pid = recibir_pid(socket_cliente);
    char* path = recibir_mensaje(socket_cliente, logger);

    char* base_path = config_get_string_value(config_km, "SCRIPTS_BASEPATH");
    char* path_completo = string_from_format("%s%s", base_path, path);

    log_debug(logger, "BASE:[%s] - PATH COMPLETO:[%s]",base_path,path_completo);
    
    FILE* archivo = fopen(path_completo, "r");
    
    // Si falló el archivo, liberamos de forma segura y salimos
    if (archivo == NULL) {
        log_error(logger, "No se pudo abrir el archivo: %s", path_completo);
        enviar_op_code(NOTOK, socket_cliente);
        free(path);
        free(path_completo);
        return;
    }

    t_list* instrucciones = list_create();
    char linea[256];  
   
    while (fgets(linea, sizeof(linea), archivo) != NULL) 
    { 
      
        char* instruccion_duplicada = strdup(linea); 
        string_trim(&instruccion_duplicada); 
        list_add(instrucciones, instruccion_duplicada);
    }

    fclose(archivo);
    
    t_proceso* proceso = malloc(sizeof(t_proceso));
    
    proceso->pid = pid;
    proceso->instrucciones = instrucciones;

    pthread_mutex_lock(&mutex_procesos);
    list_add(lista_procesos, proceso);
    pthread_mutex_unlock(&mutex_procesos);

    agregar_contexto(pid);

    log_info(logger, "## PID: %d - Proceso Creado Exitosamente", pid);

    enviar_op_code(OK, socket_cliente);
    free(path);
    free(path_completo);

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

    int pid = *(int*) list_get(paquete, 0);

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

    t_contexto* contexto = NULL;


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

    //devolver la memoria a la lista de huecos y liberar bloques de SWAP.
    for (int i = 0; i < list_size(contexto->tabla_segmentos); i++) {
    t_segmento_aux* seg = list_get(contexto->tabla_segmentos, i);

    if (seg->en_swap) {
        liberar_bloques_swap(seg->bloque_swap, seg->cantidad_bloques);
    } else {
        liberar_espacio_en_huecos(seg->direccion_base, seg->limite);
    }


        //liberar tabla de segmentos
        list_destroy_and_destroy_elements(contexto->tabla_segmentos, free);

        //liberar contexto
        free(contexto);
    }

 }
    //libero el paquete recibido por socket
    list_destroy_and_destroy_elements(paquete, free);
}

void manejar_pedido_instruccion_cpu(int socket_cliente) {
    // Recibimos el paquete de la CPU
    
    log_debug(logger,"Se Ingreso a menjar_pedido_instruccion_cpu");

    int pid = recibir_pid(socket_cliente);
    uint32_t pc = (uint32_t) recibir_int(socket_cliente);

    // Buscamos el proceso
    pthread_mutex_lock(&mutex_procesos);
    int indice = buscar_indice_proceso(pid);
    
    if (indice == -1) {
        pthread_mutex_unlock(&mutex_procesos);
        log_error(logger, "CPU pidió instrucción para PID: %d inexistente", pid);
        // Podrías enviar un op_code de error aquí si tu protocolo lo requiere
        return;
    }

    t_proceso* proceso = list_get(lista_procesos, indice);

    // 5. Verificamos que el PC esté dentro del rango de instrucciones
    if (pc >= (uint32_t)list_size(proceso->instrucciones)) {
        pthread_mutex_unlock(&mutex_procesos);
        log_error(logger, "PID: %d - PC %d fuera de rango", pid, pc);
        return;
    }

    // Obtenemos la instrucción
    char* instruccion = (char*)list_get(proceso->instrucciones, pc);
    pthread_mutex_unlock(&mutex_procesos);

    // Aplicamos el DELAY obligatorio según configuración
    int delay = config_get_int_value(config_km, "INSTRUCTION_DELAY");
    if (delay > 0) {
        usleep(delay * 1000);
    }
    enviar_mensaje(instruccion,socket_cliente);
    
    log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s", pid, pc, instruccion);

}

//Se conecta con ks: stdout
void lectura_memoria(int socket_ks) {

    enviar_op_code(OK, socket_ks);

    int pid = recibir_pid(socket_ks);

    enviar_op_code(OK, socket_ks);

    uint32_t dir_fisica = recibir_int(socket_ks);

    enviar_op_code(OK, socket_ks);

    uint32_t tamanio = recibir_int(socket_ks);

    t_memory_stick_nodo* ms = buscar_ms_por_direccion_global(dir_fisica);

    if (ms == NULL) {
        log_error(logger, "## Error: Dirección %u inexistente", dir_fisica);
        enviar_mensaje("", socket_ks);
        return;
    }

    uint32_t dir_local = dir_fisica - ms->base_global;

    if (dir_local + tamanio > ms->tamanio) {
        log_error(logger,
                  "## Error: Intento de lectura fuera de límites en MS");
        enviar_mensaje("", socket_ks);
        return;
    }

    usleep(config_get_int_value(config_km, "INSTRUCTION_DELAY") * 1000);

    /* ----------  comunicación con Memory Stick ---------- */

    enviar_op_code(LEER_MEMORIA, ms->socket_fd);
    enviar_int(dir_fisica, ms->socket_fd);
    enviar_int(tamanio, ms->socket_fd);

    void* buffer = malloc(tamanio);

    if (recv(ms->socket_fd, buffer, tamanio, MSG_WAITALL) != tamanio) {
        log_error(logger, "## Error de comunicación con Memory Stick");
        free(buffer);
        enviar_mensaje("", socket_ks);
        return;
    }

    log_info(logger,
         "## Lectura Exitosa - PID:%d - Dir Global:%u",
         pid,
         dir_fisica);

    /* ---------- Respuesta a Kernel Scheduler ---------- */

    if (send(socket_ks, buffer, tamanio, 0) != (ssize_t)tamanio) {
        log_error(logger, "Error enviando datos a Kernel Scheduler");
    }

    free(buffer);
}

void escritura_memoria(int socket_ks) {

    log_debug(logger, "[KM] entró a la funcion de escritura");

    uint32_t dir_fisica = recibir_int(socket_ks);
    uint32_t tamanio = recibir_int(socket_ks);

    int size_buffer;
    void* datos = recibir_buffer(&size_buffer, socket_ks);

    int pid = recibir_int(socket_ks);

    log_debug(logger,
        "KM_IO_STDIN recibido -> PID:%d | DirFisica:%u | Tamaño:%u | SizeBuffer:%d | Datos:\"%s\"",
        pid,
        dir_fisica,
        tamanio,
        size_buffer,
        (char*)datos);

    t_memory_stick_nodo* ms = buscar_ms_por_direccion_global(dir_fisica);

    int confirmacion = -1;

    if (ms != NULL) {

        uint32_t dir_local = dir_fisica - ms->base_global;

        if (dir_local + tamanio <= ms->tamanio) {

            usleep(config_get_int_value(config_km, "INSTRUCTION_DELAY") * 1000);

            enviar_op_code(ESCRIBIR_MEMORIA, ms->socket_fd);
            enviar_int(dir_fisica, ms->socket_fd);
            enviar_int(tamanio, ms->socket_fd);
            send(ms->socket_fd, datos, tamanio, 0);

            op_code respuesta = recibir_op_code(ms->socket_fd);

            if (respuesta == OK) {
                log_info(logger,
                         "## Escritura Exitosa - Dir Global: %u",
                         dir_fisica);
                confirmacion = 1;
            }
        }
    }

    enviar_int(confirmacion, socket_ks);

    free(datos);
}




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
        
        //tabla segmentos
        pthread_mutex_unlock(&mutex_contextos);
        log_info(logger, "## Contexto Resguardado Completo - PID: %d - PC: %u", pid, ctx->pc);
    } else {
        log_error(logger, "No se encontró el contexto para el PID: %d al intentar resguardar", pid);
    }

    // Liberamos toda la memoria de la lista y sus elementos (incluyendo los registros que ya copiamos)
    list_destroy_and_destroy_elements(paquete, free);
}

extern int socket_memory_stick;
extern t_list* lista_global_procesos;


bool _es_mas_chico(void* h1, void* h2) { 
    return ((t_hueco*)h1)->tamanio < ((t_hueco*)h2)->tamanio; 
}
bool _es_mas_grande(void* h1, void* h2) { 
    return ((t_hueco*)h1)->tamanio > ((t_hueco*)h2)->tamanio; 
}
bool _ordenar_por_base(void* seg1, void* seg2) { 
    return ((t_segmento_aux*)seg1)->direccion_base < ((t_segmento_aux*)seg2)->direccion_base; 
}

void conexion_memory_stick(int socket_ms) {
    
    uint32_t tamanio_recibido = 0;

    tamanio_recibido = recibir_int(socket_ms);
    char* ip = recibir_mensaje(socket_ms,logger);
    char* port = recibir_mensaje(socket_ms,logger);

    t_memory_stick_nodo* nuevo_ms = malloc(sizeof(t_memory_stick_nodo));
    nuevo_ms->socket_fd = socket_ms;
    nuevo_ms->tamanio = tamanio_recibido;
    nuevo_ms->ip = ip;
    nuevo_ms->port = port;


    pthread_mutex_lock(&mutex_ms);
        nuevo_ms->base_global = memoria_total_sistema;
        memoria_total_sistema += tamanio_recibido;
        list_add(lista_memory_sticks, nuevo_ms);
    pthread_mutex_unlock(&mutex_ms);


    //TEMA DE LA BASE
    t_info_memory_stick info;

    info.base = nuevo_ms->base_global;
    info.tamanio = nuevo_ms->tamanio;

    enviar_int(info.base,socket_ms);
    enviar_int(info.tamanio,socket_ms);


    log_info(logger,
    "MS conectado: base=%u tamaño=%u",
    nuevo_ms->base_global,
    nuevo_ms->tamanio);


    // Inicializamos o expandimos el espacio libre mapeado en KM
    pthread_mutex_lock(&mutex_lista_libres);
   

    t_hueco* nuevo_hueco = malloc(sizeof(t_hueco));
    nuevo_hueco->direccion_base = nuevo_ms->base_global;
    nuevo_hueco->tamanio = nuevo_ms->tamanio;

    list_add(lista_huecos_libres, nuevo_hueco);

    // Mantener los huecos ordenados por dirección base
    list_sort(lista_huecos_libres, _ordenar_huecos_por_base);


    pthread_mutex_unlock(&mutex_lista_libres);

    log_info(logger, "## Memory Stick de %u bytes Conectada", tamanio_recibido);

    enviar_op_code(NUEVA_MEMORY_STICK,socket_kernel_scheduler);

    enviar_mensaje(nuevo_ms->ip,socket_kernel_scheduler);

    enviar_mensaje(nuevo_ms->port,socket_kernel_scheduler);

    enviar_int(nuevo_ms->base_global,socket_kernel_scheduler);
    
    enviar_int(nuevo_ms->tamanio,socket_kernel_scheduler);


}


static bool mem_corrupt_notificado = false;
static pthread_mutex_t mutex_mem_corrupt = PTHREAD_MUTEX_INITIALIZER;

void manejar_caida_memory_stick(t_memory_stick_nodo* ms)
{
    pthread_mutex_lock(&mutex_mem_corrupt);

    if (!mem_corrupt_notificado) {
        mem_corrupt_notificado = true;

        log_error(logger,
            "## Memory Stick desconectada. Memoria corrupta.");

        if (socket_kernel_scheduler >= 0) {
            enviar_op_code(MEM_CORRUPT, socket_kernel_scheduler);
        }
    }

    pthread_mutex_unlock(&mutex_mem_corrupt);

    pthread_mutex_lock(&mutex_ms);
    list_remove_element(lista_memory_sticks, ms);
    pthread_mutex_unlock(&mutex_ms);

    close(ms->socket_fd);
}


// bool desconectada;

// void manejar_caida_memory_stick(t_memory_stick_nodo* ms)
// {
//     bool avisar_ks = false;
//     int fd_a_cerrar = -1;

//     pthread_mutex_lock(&mutex_ms);

//     if (!ms->desconectada) 
//     {
//         ms->desconectada = true;
//         fd_a_cerrar = ms->socket_fd;
//         ms->socket_fd = -1;

//         list_remove_element(lista_memory_sticks, ms);
//     }

//     pthread_mutex_unlock(&mutex_ms);

//     if (fd_a_cerrar != -1) {
//         close(fd_a_cerrar);
//     }

//     pthread_mutex_lock(&mutex_mem_corrupt);

//     if (!mem_corrupt_notificado) {
//         mem_corrupt_notificado = true;
//         avisar_ks = true;
//     }

//     pthread_mutex_unlock(&mutex_mem_corrupt);

//     if (avisar_ks && socket_kernel_scheduler >= 0) {
//         log_error(logger, "## Memory Stick desconectada. Memoria corrupta.");
//         enviar_op_code(MEM_CORRUPT, socket_kernel_scheduler);
//     }
// }


t_memory_stick_nodo* buscar_ms_por_direccion_global(uint32_t dir_global) {
    pthread_mutex_lock(&mutex_ms);
    for (int i = 0; i < list_size(lista_memory_sticks); i++) {
        t_memory_stick_nodo* ms = list_get(lista_memory_sticks, i);
        if (dir_global >= ms->base_global && dir_global < (ms->base_global + ms->tamanio)) {
            pthread_mutex_unlock(&mutex_ms);
            return ms;
        }
    }
    pthread_mutex_unlock(&mutex_ms);
    return NULL;
}

void* leer_bytes_globales(uint32_t dir_global, uint32_t tamanio) {
    t_memory_stick_nodo* ms = buscar_ms_por_direccion_global(dir_global);
    if (!ms) return NULL;

    uint32_t dir_local = dir_global - ms->base_global; // La resta clave traductora

    t_paquete* paquete_lectura = crear_paquete(LEER_MEMORIA);
    agregar_a_paquete(paquete_lectura, &dir_local, sizeof(uint32_t));
    agregar_a_paquete(paquete_lectura, &tamanio, sizeof(uint32_t));
    enviar_paquete(paquete_lectura, ms->socket_fd);
    eliminar_paquete(paquete_lectura);

    void* buffer = malloc(tamanio);
    recv(ms->socket_fd, buffer, tamanio, MSG_WAITALL); // Trae los bytes de la MS remota
    return buffer;
}

void escribir_bytes_globales(uint32_t dir_global, uint32_t tamanio, void* datos) {
    t_memory_stick_nodo* ms = buscar_ms_por_direccion_global(dir_global);
    if (!ms) return;

    uint32_t dir_local = dir_global - ms->base_global;

    t_paquete* paquete_escritura = crear_paquete(ESCRIBIR_MEMORIA);
    agregar_a_paquete(paquete_escritura, &dir_local, sizeof(uint32_t));
    agregar_a_paquete(paquete_escritura, &tamanio, sizeof(uint32_t));
    agregar_a_paquete(paquete_escritura, datos, tamanio);
    enviar_paquete(paquete_escritura, ms->socket_fd);
    eliminar_paquete(paquete_escritura);

    op_code res;
    recv(ms->socket_fd, &res, sizeof(op_code), MSG_WAITALL); // Espera el OK de guardado de la MS
}

int calcular_espacio_libre_total() {
    int acumulado = 0;
    pthread_mutex_lock(&mutex_lista_libres);
    for(int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* h = list_get(lista_huecos_libres, i);
        acumulado += h->tamanio;
    }
    pthread_mutex_unlock(&mutex_lista_libres);
    return acumulado;
}

t_hueco* seleccionar_hueco_segun_algoritmo(uint32_t tamanio_solicitado) 
{
    
    char* estrategia = config_get_string_value(config_km, "ALLOCATION_STRATEGY"); // Lee "BEST" de tu config
    t_hueco* elegido = NULL;

    t_list* huecos_validos = list_create();

    for(int i = 0; i < list_size(lista_huecos_libres); i++) {
        t_hueco* h = list_get(lista_huecos_libres, i);
       
        if(h->tamanio >= tamanio_solicitado) {
            list_add(huecos_validos, h);
        }
    }

    if (list_is_empty(huecos_validos)) {
        list_destroy(huecos_validos);
        return NULL; // Se queda sin bache contiguo -> Gatillo de compactación
    }

    if (strcmp(estrategia, "BEST") == 0) {
        list_sort(huecos_validos, _es_mas_chico);
    } else if (strcmp(estrategia, "WORST") == 0) {
        list_sort(huecos_validos, _es_mas_grande);
    }

    elegido = list_get(huecos_validos, 0);
    list_destroy(huecos_validos);
    return elegido;
}

void ejecutar_compactacion_fisica_memory_stick() {
    log_info(logger, "## [COMPACTACIÓN] Iniciando reorganización física de la memoria...");

    t_list* todos_los_segmentos = list_create();

    pthread_mutex_lock(&mutex_contextos);
    for (int i = 0; i < list_size(lista_contextos); i++) {
        t_contexto* ctx = list_get(lista_contextos, i);
        list_add_all(todos_los_segmentos, ctx->tabla_segmentos);
    }
    pthread_mutex_unlock(&mutex_contextos);

    list_sort(todos_los_segmentos, _ordenar_por_base);
    uint32_t proxima_direccion_libre = 0;

    for (int i = 0; i < list_size(todos_los_segmentos); i++) {
        t_segmento_aux* seg = list_get(todos_los_segmentos, i);

        if (seg->direccion_base != proxima_direccion_libre) {
            log_info(logger, "## Mudando Segmento %d de Base %u a Nueva Base %u", seg->id_segmento, seg->direccion_base, proxima_direccion_libre);

            // Se hace la mudanza de datos real por red entre las MS
            void* bytes_datos = leer_bytes_globales(seg->direccion_base, seg->limite);
            escribir_bytes_globales(proxima_direccion_libre, seg->limite, bytes_datos);
            free(bytes_datos);

            seg->direccion_base = proxima_direccion_libre; // Cambiamos la tabla lógica
        }
        proxima_direccion_libre += seg->limite;
    }

    // Unificamos toda la fragmentación en un único gran hueco al fondo del estante
    pthread_mutex_lock(&mutex_lista_libres);
    list_clean_and_destroy_elements(lista_huecos_libres, free);

    if (proxima_direccion_libre < memoria_total_sistema) {
        t_hueco* gran_hueco_final = malloc(sizeof(t_hueco));
        gran_hueco_final->direccion_base = proxima_direccion_libre;
        gran_hueco_final->tamanio = memoria_total_sistema - proxima_direccion_libre;
        list_add(lista_huecos_libres, gran_hueco_final);
    }
    pthread_mutex_unlock(&mutex_lista_libres);

    // Retardo Obligatorio de tu config (Lee el COMPACTION_DELAY=30000)
    int delay = config_get_int_value(config_km, "COMPACTION_DELAY");
    usleep(delay * 1000); 

    log_info(logger, "## Compactación física en las Memory Sticks finalizada.");
    list_destroy(todos_los_segmentos);
}

void solicitar_y_ejecutar_compactacion(int socket_ks) {
    log_warning(logger, "## Memoria fragmentada. Solicitando desalojo al Kernel Scheduler...");

    enviar_op_code(DESALOJO, socket_ks);

    // Bloqueo de sincronización: Queda esperando que KS eche a los procesos de las CPU
    op_code respuesta_ks;
    recv(socket_ks, &respuesta_ks, sizeof(op_code), MSG_WAITALL);

    if (respuesta_ks == CPUS_DESALOJADAS_OK) {
        log_info(logger, "## Kernel Scheduler dio el OK. Iniciando mudanza física...");
        
        ejecutar_compactacion_fisica_memory_stick();

        t_paquete* paquete_fin = crear_paquete(COMPACTACION_FINALIZADA);
        enviar_paquete(paquete_fin, socket_ks);
        eliminar_paquete(paquete_fin);
        log_info(logger, "## Fin de compactación notificado. Sistema reactivado.");
    }
}


void creacion_segmento(int socket_cliente, int socket_ks, int pid, int id_segmento, uint32_t tamanio_segmento) {

    int max_segment_size = config_get_int_value(config_km, "SEGMENT_MAX_SIZE");


    if (tamanio_segmento > (uint32_t) max_segment_size) 
    {
        log_error(
            logger,
            "## PID: %d - Segmento %d excede tamaño máximo permitido: %u > %d",
            pid,
            id_segmento,
            tamanio_segmento,
            max_segment_size
        );

        int error = -1;
        enviar_op_code(NOTOK,socket_cliente);
        return;
    }
    
    pthread_mutex_lock(&mutex_lista_libres);
    t_hueco* bache_elegido = seleccionar_hueco_segun_algoritmo(tamanio_segmento);
    pthread_mutex_unlock(&mutex_lista_libres);

    if (bache_elegido == NULL) {
        
        int espacio_total_disponible = calcular_espacio_libre_total();


        log_debug(logger,
        "Espacio libre total: %d - Pedido: %u",
        espacio_total_disponible,
        tamanio_segmento);
        
        if (espacio_total_disponible >= tamanio_segmento) 
        {
            // No está lleno, está fragmentado -> SE LANZA TODO EL FLUJO AUTOMÁTICO
            solicitar_y_ejecutar_compactacion(socket_ks);

            // Post-compactación: Buscamos de nuevo (ahora estará consolidado al fondo)
            pthread_mutex_lock(&mutex_lista_libres);
            bache_elegido = seleccionar_hueco_segun_algoritmo(tamanio_segmento);
            pthread_mutex_unlock(&mutex_lista_libres);
        } 
        else 
        {
            log_error(logger, "## Out of Memory real - PID: %d - Tamaño: %u", pid, tamanio_segmento);
            int error = -1;
            enviar_op_code(NOTOK,socket_cliente);
            return;
        }

    }

    // Recortamos el bache que tomamos para el nuevo segmento
    pthread_mutex_lock(&mutex_lista_libres);
    t_segmento_aux* nuevo_segmento = malloc(sizeof(t_segmento_aux));
    nuevo_segmento->id_segmento = id_segmento;
    nuevo_segmento->direccion_base = bache_elegido->direccion_base;
    nuevo_segmento->limite = tamanio_segmento;

    bache_elegido->direccion_base += tamanio_segmento;
    bache_elegido->tamanio -= tamanio_segmento;

    if (bache_elegido->tamanio == 0) {
        list_remove_element(lista_huecos_libres, bache_elegido);
        free(bache_elegido);
    }
    pthread_mutex_unlock(&mutex_lista_libres);

    // Guardamos las estructuras lógicas del proceso de segmentación pura
    pthread_mutex_lock(&mutex_contextos);
    t_contexto* ctx = buscar_contexto(pid);
    if (ctx != NULL) {
        list_add(ctx->tabla_segmentos, nuevo_segmento);
    }
    pthread_mutex_unlock(&mutex_contextos);

    log_info(logger, "## PID: %d - Segmento Creado %d - Tamaño: %u", pid, id_segmento, tamanio_segmento);



    int ok = 1;
    enviar_op_code(OK,socket_cliente);

    enviar_int(nuevo_segmento->direccion_base,socket_cliente);
}

void eliminar_segmento(int pid, int id_segmento) {
    pthread_mutex_lock(&mutex_contextos);
    t_contexto* ctx = buscar_contexto(pid);
    
    if (ctx == NULL) {
        pthread_mutex_unlock(&mutex_contextos);
        log_error(logger, "## Error: No se encontró el contexto del PID %d para eliminar segmento", pid);
        return;
    }

    t_segmento_aux* seg_a_eliminar = NULL;
    int indice_seg = -1;

    for (int i = 0; i < list_size(ctx->tabla_segmentos); i++) {
        t_segmento_aux* seg = list_get(ctx->tabla_segmentos, i);
        if (seg->id_segmento == id_segmento) {
            seg_a_eliminar = seg;
            indice_seg = i;
            break;
        }
    }

    if (seg_a_eliminar == NULL) {
        pthread_mutex_unlock(&mutex_contextos);
        log_error(logger, "## Error: No se encontró el Segmento %d en el PID %d", id_segmento, pid);
        return;
    }

    //Sacamos el segmento de la tabla del proceso
    list_remove(ctx->tabla_segmentos, indice_seg);
    pthread_mutex_unlock(&mutex_contextos);


    liberar_espacio_en_huecos(seg_a_eliminar->direccion_base, seg_a_eliminar->limite);

    log_info(logger, "## PID: %d - Segmento %d Liberado (Base: %u, Tamaño: %u)", 
             pid, id_segmento, seg_a_eliminar->direccion_base, seg_a_eliminar->limite);

    free(seg_a_eliminar);
}
// Mueve la función comparadora fuera del ámbito de la función principal
bool _ordenar_huecos_por_base(void* h1, void* h2) {
    return ((t_hueco*)h1)->direccion_base < ((t_hueco*)h2)->direccion_base;
}

void liberar_espacio_en_huecos(uint32_t direccion_base, uint32_t tamanio) {
    pthread_mutex_lock(&mutex_lista_libres);

    t_hueco* nuevo_hueco = malloc(sizeof(t_hueco));
    nuevo_hueco->direccion_base = direccion_base;
    nuevo_hueco->tamanio = tamanio;
    list_add(lista_huecos_libres, nuevo_hueco);

    // Ahora la función es visible aquí
    list_sort(lista_huecos_libres, _ordenar_huecos_por_base);

    //El bucle de consolidación parece correcto
    for (int i = 0; i < list_size(lista_huecos_libres) - 1; i++) {
        t_hueco* actual = list_get(lista_huecos_libres, i);
        t_hueco* siguiente = list_get(lista_huecos_libres, i + 1);

        if (actual->direccion_base + actual->tamanio == siguiente->direccion_base) {
            actual->tamanio += siguiente->tamanio;
            // Verifica si tu list_remove libera memoria o solo el nodo
            list_remove(lista_huecos_libres, i + 1); 
            free(siguiente);
            i--; 
        }
    }
    pthread_mutex_unlock(&mutex_lista_libres);
}

t_contexto* buscar_contexto(int pid) {

    for(int i = 0; i < list_size(lista_contextos); i++) {

        t_contexto* contexto = list_get(lista_contextos, i);

        if(contexto->pid == pid) {
            return contexto;
        }
    }

    return NULL;
}

void enviar_contexto_cpu(int socket_cpu, int pid) {

    t_contexto* contexto = buscar_contexto(pid);

    if(contexto == NULL) {
        log_error(logger, "No existe contexto PID %d", pid);
        return;
    }

    t_paquete* paquete = crear_paquete(CONTEXTO);

    agregar_a_paquete(paquete, &contexto->pid, sizeof(int));
    agregar_a_paquete(paquete, &contexto->pc, sizeof(uint32_t));

    agregar_a_paquete(paquete, &contexto->ax, sizeof(uint8_t));
    agregar_a_paquete(paquete, &contexto->bx, sizeof(uint8_t));
    agregar_a_paquete(paquete, &contexto->cx, sizeof(uint8_t));
    agregar_a_paquete(paquete, &contexto->dx, sizeof(uint8_t));

    agregar_a_paquete(paquete, &contexto->eax, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto->ebx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto->ecx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto->edx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto->si, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto->di, sizeof(uint32_t));

    int cantidad_segmentos = list_size(contexto->tabla_segmentos);
    agregar_a_paquete(paquete, &cantidad_segmentos, sizeof(int));

    for(int i = 0; i < cantidad_segmentos; i++) {

        t_segmento_aux* seg = list_get(contexto->tabla_segmentos, i);

        agregar_a_paquete(paquete, &seg->id_segmento, sizeof(int));
        agregar_a_paquete(paquete, &seg->direccion_base, sizeof(uint32_t));
        agregar_a_paquete(paquete, &seg->limite, sizeof(uint32_t));
    }

    enviar_paquete(paquete, socket_cpu);
    eliminar_paquete(paquete);

    log_info(logger, "Contexto enviado PID %d con %d segmentos", pid, cantidad_segmentos);
}

void recibir_contexto_cpu(int socket_cpu) {
    log_debug(logger, "[recibir_contexto_cpu] llegó aca");

    int size;

    int pid = recibir_int(socket_cpu);

    t_contexto* contexto = buscar_contexto(pid);

    if (contexto == NULL) {

        log_error(logger, "No existe contexto para PID %d", pid);

        enviar_op_code(NOTOK, socket_cpu);
        return;
    }

    // PC
    contexto->pc = recibir_int(socket_cpu);

    // Registros de 8 bits
    uint8_t* ax = recibir_buffer(&size, socket_cpu);
    uint8_t* bx = recibir_buffer(&size, socket_cpu);
    uint8_t* cx = recibir_buffer(&size, socket_cpu);
    uint8_t* dx = recibir_buffer(&size, socket_cpu);

    contexto->ax = *ax;
    contexto->bx = *bx;
    contexto->cx = *cx;
    contexto->dx = *dx;

    free(ax);
    free(bx);
    free(cx);
    free(dx);

    // Registros de 32 bits
    contexto->eax = recibir_int(socket_cpu);
    contexto->ebx = recibir_int(socket_cpu);
    contexto->ecx = recibir_int(socket_cpu);
    contexto->edx = recibir_int(socket_cpu);
    contexto->si  = recibir_int(socket_cpu);
    contexto->di  = recibir_int(socket_cpu);

    int cantidad_segmentos = recibir_int(socket_cpu);

    if (contexto->tabla_segmentos != NULL)
        list_destroy_and_destroy_elements(contexto->tabla_segmentos, free);

    contexto->tabla_segmentos = list_create();

    for (int i = 0; i < cantidad_segmentos; i++) {

        t_segmento* segmento = malloc(sizeof(t_segmento));

        segmento->id_segmento = recibir_int(socket_cpu);
        segmento->base        = recibir_int(socket_cpu);
        segmento->tamanio     = recibir_int(socket_cpu);

        list_add(contexto->tabla_segmentos, segmento);
    }

    log_info(logger,
             "Contexto actualizado PID %d con %d segmentos",
             pid,
             cantidad_segmentos);

    enviar_op_code(OK, socket_cpu);
}



// CONEXION CON SWAP
// Buscar bloques consecutivos (Algoritmo First-Fit)
int obtener_n_bloques_libres(int n) {
    int contador = 0;
    int inicio = -1;

    for (int i = 0; i < total_bloques_swap; i++) {
        if (!bitarray_test_bit(bitmap_swap, i)) {
            if (contador == 0) inicio = i;
            contador++;
            if (contador == n) {
                // Marcar como ocupados
                for (int j = inicio; j < inicio + n; j++) 
                    bitarray_set_bit(bitmap_swap, j);
                return inicio;
            }
        } else {
            contador = 0;
            inicio = -1;
        }
    }
    return -1; // No hay espacio suficiente
}

// Liberar bloques al volver a RAM
void liberar_bloques_swap(int nro_bloque, int cantidad) {
    for (int i = nro_bloque; i < nro_bloque + cantidad; i++) {
        bitarray_clean_bit(bitmap_swap, i);
    }
}

void mover_segmento_a_swap(t_segmento_aux* seg) {
    int bloques_necesarios = ceil((double)seg->limite / block_size_swap);
    int primer_bloque = obtener_n_bloques_libres(bloques_necesarios);

    if (primer_bloque == -1) {
        log_error(logger, "SWAP LLENO: No se pudo suspender PID %d", seg->id_segmento);
        return;
    }

    void* buffer = leer_bytes_globales(seg->direccion_base, seg->limite);
    
    for (int i = 0; i < bloques_necesarios; i++) {
        // Calculamos cuánto leer: si es el último bloque, puede que no sea completo
        int a_escribir = (i == bloques_necesarios - 1) ? 
                         (seg->limite - (i * block_size_swap)) : block_size_swap;
        
        void* bloque_data = calloc(1, block_size_swap);
        memcpy(bloque_data, buffer + (i * block_size_swap), a_escribir);
        
        enviar_a_swap(primer_bloque + i, bloque_data);
        free(bloque_data);
    }
    
    seg->bloque_swap = primer_bloque;
    seg->cantidad_bloques = bloques_necesarios;
    seg->en_swap = true;
    
    liberar_espacio_en_huecos(seg->direccion_base, seg->limite);
    free(buffer);
}
void suspender_proceso(int pid) {
    t_contexto* ctx = buscar_contexto(pid); 
    if (!ctx) return;

    for (int i = 0; i < list_size(ctx->tabla_segmentos); i++) {
        t_segmento_aux* seg = list_get(ctx->tabla_segmentos, i);
        if (!seg->en_swap) {
            mover_segmento_a_swap(seg);
        }
    }
    log_info(logger, "Proceso %d movido totalmente a SWAP", pid);
}
int recibir_de_swap(t_segmento_aux* seg, void* buffer_destino)
{
    int offset = 0;

    for (int i = 0; i < seg->cantidad_bloques; i++) {
        int numero_bloque = seg->bloque_swap + i;

        t_paquete * paquete = crear_paquete(LECTURA_BLOQUE);
        agregar_a_paquete(paquete, &numero_bloque, sizeof(int));
        enviar_paquete(paquete, socket_swap);
        eliminar_paquete(paquete);

        int cod_op = recibir_op_code(socket_swap);

        if (cod_op != RESPUESTA_DATOS) {
            log_error(logger,
                "Error al leer bloque %d de SWAP. Opcode recibido: %d",
                numero_bloque, cod_op);
            return -1;
        }

        t_list* respuesta = recibir_paquete(socket_swap);

        if (respuesta == NULL || list_size(respuesta) != 1) {
            log_error(logger,
                "Respuesta inválida al leer bloque %d de SWAP",
                numero_bloque);

            if (respuesta != NULL) {
                list_destroy_and_destroy_elements(respuesta, free);
            }

            return -1;
        }

        void* bloque = list_get(respuesta, 0);

        int bytes_restantes = seg->limite - offset;
        int bytes_a_copiar = bytes_restantes < block_size_swap
            ? bytes_restantes
            : block_size_swap;

        memcpy((char*) buffer_destino + offset, bloque, bytes_a_copiar);
        offset += bytes_a_copiar;

        list_destroy_and_destroy_elements(respuesta, free);
    }

    liberar_bloques_swap(seg->bloque_swap, seg->cantidad_bloques);
    seg->en_swap = false;

    log_info(logger, "Segmento %d recuperado de SWAP", seg->id_segmento);

    return 0;
}

void enviar_a_swap(int nro_bloque, void* datos) {
    //  Crear y enviar el paquete
    t_paquete * paquete = crear_paquete(ESCRITURA_BLOQUE);
    
    agregar_a_paquete(paquete, &nro_bloque, sizeof(int));
    agregar_a_paquete(paquete, datos, block_size_swap);
    
    enviar_paquete(paquete, socket_swap);
    eliminar_paquete(paquete);
    
    //  Esperar confirmación del SWAP
    int cod_op = recibir_op_code(socket_swap);
    
    //  Manejo de estados de la conexión
    if (cod_op == RESPUESTA_OK) {
        log_info(logger, "## Escritura exitosa: bloque %d", nro_bloque);
    } 
    else if (cod_op == RESPUESTA_ERROR) {
    log_error(logger, "## Error: El SWAP rechazó la escritura del bloque %d", nro_bloque);
    }

    else if (cod_op == -1) {
        log_error(logger, "## Error Crítico: SWAP desconectado inesperadamente al escribir bloque %d", nro_bloque);
        // Opcional: abortar, cerrar socket o intentar reconectar
        close(socket_swap);
        socket_swap = -1; // Marcamos el socket como inválido
    } 
    else {
        log_error(logger, "## Error: Código de operación inesperado recibido del SWAP: %d", cod_op);
    }
}

int desuspender_proceso(int pid) {
    t_contexto* ctx = buscar_contexto(pid);

    if (!ctx) {
        log_error(logger, "No existe el proceso %d para desuspender", pid);
        return -1;
    }

    log_info(logger, "Des-suspendiendo proceso %d...", pid);

    for (int i = 0; i < list_size(ctx->tabla_segmentos); i++) {
        t_segmento_aux* seg = list_get(ctx->tabla_segmentos, i);

        if (seg->en_swap) {
            pthread_mutex_lock(&mutex_lista_libres);

            t_hueco* hueco = seleccionar_hueco_segun_algoritmo(seg->limite);

            if (hueco == NULL) {
                pthread_mutex_unlock(&mutex_lista_libres);
                log_error(
                    logger,
                    "ERROR: No hay espacio en RAM para el segmento %d del proceso %d",
                    seg->id_segmento,
                    pid
                );
                return -1;
            }

            int nueva_base = hueco->direccion_base;

            hueco->direccion_base += seg->limite;
            hueco->tamanio -= seg->limite;

            if (hueco->tamanio == 0) {
                list_remove_element(lista_huecos_libres, hueco);
                free(hueco);
            }

            pthread_mutex_unlock(&mutex_lista_libres);

            void* buffer = malloc(seg->limite);
            if (recibir_de_swap(seg, buffer) != 0) {
                free(buffer);
                liberar_espacio_en_huecos(nueva_base, seg->limite);
                return -1;
            }

            escribir_bytes_globales(nueva_base, seg->limite, buffer);

            seg->direccion_base = nueva_base;
            seg->en_swap = false;

            free(buffer);

            log_info(
                logger,
                "Segmento %d restaurado en dirección %d",
                seg->id_segmento,
                nueva_base
            );
        }
    }

    log_info(logger, "Proceso %d des-suspendido exitosamente", pid);
    return 0;
}

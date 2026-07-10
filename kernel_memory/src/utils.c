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
#include "../../utils/src/global_utils.h"


//“almacenar un contexto de ejecución por PID”


//“Este módulo deberá mantener el contexto de ejecución de cada Proceso del sistema”
//-> tengo que guardar TODOS los contextos de TODOS los procesos
// global pq la usan varias partes del programa:crear proceso, cpu, finalizar proceso.

//inicializacion que va en el main EN UTILS.C
// Al principio de tu utils.c, las variables globales van declaradas sin asignarles funciones:
pthread_mutex_t mutex_lista_libres;
t_list* lista_huecos_libres;

t_list* lista_contextos;
pthread_mutex_t mutex_contextos;

t_list* lista_procesos;
pthread_mutex_t mutex_procesos;

t_list* lista_memory_sticks;
pthread_mutex_t mutex_ms;

// La función encierra TODO la creación de estructuras juntas
void inicializar_utils(void) {
    lista_contextos = list_create();
    pthread_mutex_init(&mutex_contextos, NULL);

    lista_procesos = list_create();
    pthread_mutex_init(&mutex_procesos, NULL); // Acá tenías un cierre '}' extra que rompía todo

    lista_memory_sticks = list_create();
    pthread_mutex_init(&mutex_ms, NULL);
    
    lista_huecos_libres = list_create();
    pthread_mutex_init(&mutex_lista_libres, NULL);
} 

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
        string_trim(&instruccion_duplicada); 
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
    // Recibimos el paquete de la CPU
    t_list* paquete = recibir_paquete(socket_cliente);
    
    // Extraemos los valores de forma segura y COPIAMOS el contenido
    // Esto es vital: si destruimos el paquete después, las variables locales sobreviven.
    int* pid_ptr = (int*)list_get(paquete, 0);
    uint32_t* pc_ptr = (uint32_t*)list_get(paquete, 1);
    
    int pid = *pid_ptr;
    uint32_t pc = *pc_ptr;
    
    // Limpiamos el paquete inmediatamente para evitar fugas
    list_destroy_and_destroy_elements(paquete, free);

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
    int delay = config_get_int_value(config, "INSTRUCTION_DELAY");
    if (delay > 0) {
        usleep(delay * 1000);
    }

    // Preparamos el envío: [TAMAÑO_INSTRUCCION][INSTRUCCION]
    int tam_instruccion = strlen(instruccion) + 1;
    int tam_total = sizeof(int) + tam_instruccion;
    void* buffer_enviar = malloc(tam_total);

    int offset = 0;
    memcpy(buffer_enviar + offset, &tam_instruccion, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer_enviar + offset, instruccion, tam_instruccion);

    // Enviamos
    send(socket_cliente, buffer_enviar, tam_total, 0);
    
    log_info(logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s", pid, pc, instruccion);

    //Limpiamos buffer local
    free(buffer_enviar);
}

//Se conecta con ks: stdout

//Se conecta con ks: stdin
//yo (tami) puse tambien el pid (te mando el pid)
void lectura_memoria(int socket_ks) {
    t_list* paquete = recibir_paquete(socket_ks);

    if (paquete == NULL || list_size(paquete) < 3) {
        log_error(logger, "## Error: Paquete de lectura inválido");
        int error = -1;
        send(socket_ks, &error, sizeof(int), 0);
        if (paquete) list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    uint32_t dir_fisica = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio    = *(uint32_t*)list_get(paquete, 2);

    t_memory_stick_nodo* ms = buscar_ms_por_direccion_global(dir_fisica);

    if (ms != NULL) {
        uint32_t dir_local = dir_fisica - ms->base_global;

        // PROTECCIÓN: Validar que el pedido no se salga de los límites de la MS
        if (dir_local + tamanio > ms->tamanio) {
            log_error(logger, "## Error: Intento de lectura fuera de límites en MS (FD: %d)", ms->socket_fd);
            int error = -1;
            send(socket_ks, &error, sizeof(int), 0);
        } else {
            // Aplicar delay si lo requiere tu configuración
            usleep(config_get_int_value(config, "INSTRUCTION_DELAY") * 1000);

            t_paquete* paquete_ms = crear_paquete(LEER_MEMORIA);
            agregar_a_paquete(paquete_ms, &dir_local, sizeof(uint32_t));
            agregar_a_paquete(paquete_ms, &tamanio, sizeof(uint32_t));
            
            enviar_paquete(paquete_ms, ms->socket_fd);
            eliminar_paquete(paquete_ms);

            void* buffer_datos = malloc(tamanio);
            if (recv(ms->socket_fd, buffer_datos, tamanio, MSG_WAITALL) > 0) {
                log_info(logger, "## Lectura Exitosa - Dir Global: %u", dir_fisica);
                send(socket_ks, buffer_datos, tamanio, 0);
            } else {
                log_error(logger, "## Error de comunicación con MS");
            }
            free(buffer_datos);
        }
    } else {
        log_error(logger, "## Error: Dirección %u inexistente", dir_fisica);
        int error = -1;
        send(socket_ks, &error, sizeof(int), 0);
    }

    list_destroy_and_destroy_elements(paquete, free);
}

void escritura_memoria(int socket_ks) {
    t_list* paquete = recibir_paquete(socket_ks);
    
    if (paquete == NULL || list_size(paquete) < 4) {
        log_error(logger, "## Error: Paquete de escritura inválido");
        int error = -1;
        send(socket_ks, &error, sizeof(int), 0);
        if (paquete) list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    uint32_t dir_fisica = *(uint32_t*)list_get(paquete, 1);
    uint32_t tamanio    = *(uint32_t*)list_get(paquete, 2);
    void* datos = list_get(paquete, 3); 

    t_memory_stick_nodo* ms = buscar_ms_por_direccion_global(dir_fisica);
    int confirmacion = -1;

    if (ms != NULL) {
        uint32_t dir_local = dir_fisica - ms->base_global;

        // PROTECCIÓN: Validar límites
        if (dir_local + tamanio <= ms->tamanio) {
            usleep(config_get_int_value(config, "INSTRUCTION_DELAY") * 1000);

            t_paquete* paquete_ms = crear_paquete(ESCRIBIR_MEMORIA);
            agregar_a_paquete(paquete_ms, &dir_local, sizeof(uint32_t));
            agregar_a_paquete(paquete_ms, &tamanio, sizeof(uint32_t));
            agregar_a_paquete(paquete_ms, datos, tamanio);
            
            enviar_paquete(paquete_ms, ms->socket_fd);
            eliminar_paquete(paquete_ms);

            op_code respuesta;
            if (recv(ms->socket_fd, &respuesta, sizeof(op_code), MSG_WAITALL) > 0 && respuesta == OK) {
                log_info(logger, "## Escritura Exitosa - Dir Global: %u", dir_fisica);
                confirmacion = 1;
            }
        }
    }

    send(socket_ks, &confirmacion, sizeof(int), 0);
    list_destroy_and_destroy_elements(paquete, free);
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


void conexion_memory_stick(int socket_ms, int socket_kernel_scheduler) {
    uint32_t tamanio_recibido = 0;

    if (recv(socket_ms, &tamanio_recibido, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(logger, "Error al recibir el tamaño del Memory Stick.");
        close(socket_ms);
        return;
    }

    t_memory_stick_nodo* nuevo_ms = malloc(sizeof(t_memory_stick_nodo));
    nuevo_ms->socket_fd = socket_ms;
    nuevo_ms->tamanio = tamanio_recibido;

    pthread_mutex_lock(&mutex_ms);
    nuevo_ms->base_global = memoria_total_sistema;
    memoria_total_sistema += tamanio_recibido;
    list_add(lista_memory_sticks, nuevo_ms);
    pthread_mutex_unlock(&mutex_ms);

    // Inicializamos o expandimos el espacio libre mapeado en KM
    pthread_mutex_lock(&mutex_lista_libres);
    if (list_is_empty(lista_huecos_libres)) {
        t_hueco* primer_hueco = malloc(sizeof(t_hueco));
        primer_hueco->direccion_base = 0;
        primer_hueco->tamanio = tamanio_recibido;
        list_add(lista_huecos_libres, primer_hueco);
    } else {
        t_hueco* ultimo_hueco = list_get(lista_huecos_libres, list_size(lista_huecos_libres) - 1);
        ultimo_hueco->tamanio += tamanio_recibido;
    }
    pthread_mutex_unlock(&mutex_lista_libres);

    log_info(logger, "## Memory Stick de %u bytes Conectada", tamanio_recibido);

    t_paquete* paquete = crear_paquete(NUEVA_MEMORIA_ACUM); 
    agregar_a_paquete(paquete, &memoria_total_sistema, sizeof(uint32_t));
    enviar_paquete(paquete, socket_kernel_scheduler);
    eliminar_paquete(paquete);
}


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

t_hueco* seleccionar_hueco_segun_algoritmo(uint32_t tamanio_solicitado) {
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

    t_paquete* paquete_desalojo = crear_paquete(DESALOJO);
    enviar_paquete(paquete_desalojo, socket_ks);
    eliminar_paquete(paquete_desalojo);

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
    
    pthread_mutex_lock(&mutex_lista_libres);
    t_hueco* bache_elegido = seleccionar_hueco_segun_algoritmo(tamanio_segmento);
    pthread_mutex_unlock(&mutex_lista_libres);

    if (bache_elegido == NULL) {
        int espacio_total_disponible = calcular_espacio_libre_total();

        if (espacio_total_disponible >= tamanio_segmento) {
            // No está lleno, está fragmentado -> SE LANZA TODO EL FLUJO AUTOMÁTICO
            solicitar_y_ejecutar_compactacion(socket_ks);

            // Post-compactación: Buscamos de nuevo (ahora estará consolidado al fondo)
            pthread_mutex_lock(&mutex_lista_libres);
            bache_elegido = seleccionar_hueco_segun_algoritmo(tamanio_segmento);
            pthread_mutex_unlock(&mutex_lista_libres);
        } else {
            log_error(logger, "## Out of Memory real - PID: %d - Tamaño: %u", pid, tamanio_segmento);
            int error = -1;
            send(socket_cliente, &error, sizeof(int), 0);
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

    log_info(logger, "## PID: %d - Segmento %d Creado en Base: %u", pid, id_segmento, nuevo_segmento->direccion_base);

    int ok = 1;
    send(socket_cliente, &ok, sizeof(int), 0);
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

  ! 
    liberar_espacio_en_huecos(seg_a_eliminar->direccion_base, seg_a_eliminar->limite);

    log_info(logger, "## PID: %d - Segmento %d Liberado (Base: %u, Tamaño: %u)", 
             pid, id_segmento, seg_a_eliminar->direccion_base, seg_a_eliminar->limite);

    free(seg_a_eliminar);
}
void liberar_espacio_en_huecos(uint32_t direccion_base, uint32_t tamanio) {
    pthread_mutex_lock(&mutex_lista_libres);

    t_hueco* nuevo_hueco = malloc(sizeof(t3-*.+_hueco));
    nuevo_hueco->direccion_base = direccion_base;
    nuevo_hueco->tamanio = tamanio;
    list_add(lista_huecos_libres, nuevo_hueco);

    bool _ordenar_huecos_por_base(void* h1, void* h2) {
        return ((t_hueco*)h1)->direccion_base < ((t_hueco*)h2)->direccion_base;
    }
    list_sort(lista_huecos_libres, _ordenar_huecos_por_base);

    for (int i = 0; i < list_size(lista_huecos_libres) - 1; i++) {
        t_hueco* actual = list_get(lista_huecos_libres, i);
        t_hueco* siguiente = list_get(lista_huecos_libres, i + 1);

        if (actual->direccion_base + actual->tamanio == siguiente->direccion_base) {
            actual->tamanio += siguiente->tamanio;
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
        log_error(logger, "No existe contexto para PID %d", pid);
        enviar_op_code(NOTOK, socket_cpu);
        return;
    }

    enviar_op_code(CONTEXTO, socket_cpu);
    t_paquete* paquete = crear_paquete(CONTEXTO);

    // Registros base
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

    // tabla de segmentos
    pthread_mutex_lock(&mutex_contextos);
    int cantidad_segmentos = list_size(contexto->tabla_segmentos);
    agregar_a_paquete(paquete, &cantidad_segmentos, sizeof(int));

   for (int i = 0; i < cantidad_segmentos; i++) {
    t_segmento_aux* seg = list_get(contexto->tabla_segmentos, i);
    agregar_a_paquete(paquete, &seg->id_segmento, sizeof(int));
    agregar_a_paquete(paquete, &seg->direccion_base, sizeof(uint32_t));
    agregar_a_paquete(paquete, &seg->limite, sizeof(uint32_t));
    agregar_a_paquete(paquete, &seg->id_ms, sizeof(int));
    }
    pthread_mutex_unlock(&mutex_contextos);


    enviar_paquete(paquete, socket_cpu);
    eliminar_paquete(paquete);

    log_info(logger, "Contexto enviado PID %d con %d segmentos", pid, cantidad_segmentos);
}
//lo recibo en el mismo orden en el que la cpu lo envia

void recibir_contexto_cpu(int socket_cpu) {

    t_list* paquete = recibir_paquete(socket_cpu);

    int pid = *(int*)list_get(paquete, 0);

    t_contexto* contexto = buscar_contexto(pid);

    if(contexto == NULL) {

        log_error(logger, "No existe contexto para PID %d", pid);

        enviar_op_code(NOTOK, socket_cpu);

        list_destroy_and_destroy_elements(paquete, free);
        return;
    }

    contexto->pc = *(uint32_t*)list_get(paquete, 1);

    // registros 8 bits
    contexto->ax = *(uint8_t*)list_get(paquete, 2);
    contexto->bx = *(uint8_t*)list_get(paquete, 3);
    contexto->cx = *(uint8_t*)list_get(paquete, 4);
    contexto->dx = *(uint8_t*)list_get(paquete, 5);

    // registros 32 bits
    contexto->eax = *(uint32_t*)list_get(paquete, 6);
    contexto->ebx = *(uint32_t*)list_get(paquete, 7);
    contexto->ecx = *(uint32_t*)list_get(paquete, 8);
    contexto->edx = *(uint32_t*)list_get(paquete, 9);
    contexto->si  = *(uint32_t*)list_get(paquete, 10);
    contexto->di  = *(uint32_t*)list_get(paquete, 11);

    log_info(logger, "Contexto actualizado PID %d", pid);

    enviar_op_code(OK, socket_cpu);

    list_destroy_and_destroy_elements(paquete, free);
}

// CONEXION CON SWAP
// Supongamos que tienes estas variables globales en tu KM
t_bitarray* bitmap_swap;
extern int total_bloques_swap;
int total_bloques_swap;

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
        log_error(logger, "SWAP LLENO: No se pudo suspender PID %d", seg->pid);
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
void recibir_de_swap(t_segmento_aux* seg, void* buffer_destino) {
    // Enviar pedido
    t_paquete* paquete = crear_paquete(); // O tu función de creación
    paquete->codigo_operacion = LECTURA_BLOQUE;
    agregar_a_paquete(paquete, &(seg->bloque_swap), sizeof(int)); // Pedimos el inicio
    enviar_paquete(paquete, socket_swap);
    eliminar_paquete(paquete);

    t_list* respuesta = recibir_paquete(socket_swap);
    int offset = 0;
    for(int i = 0; i < list_size(respuesta); i++) {
    void* bloque = list_get(respuesta, i);
    memcpy(buffer_destino + offset, bloque, block_size_swap);
    offset += block_size_swap;
}

    //  LIMPIEZA CRÍTICA (Evita Memory Leaks)
    // list_destroy_and_destroy_elements libera los elementos (datos) y la lista
    list_destroy_and_destroy_elements(respuesta, free); 
    
    // Actualizar estado
    liberar_bloques_swap(seg->bloque_swap, seg->cantidad_bloques);
    seg->en_swap = false;
    log_info(logger, "Segmento %d recuperado de SWAP", seg->id);}


void enviar_a_swap(int nro_bloque, void* datos) {
    //  Crear y enviar el paquete
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = ESCRITURA_BLOQUE;
    
    agregar_a_paquete(paquete, &nro_bloque, sizeof(int));
    agregar_a_paquete(paquete, datos, block_size_swap);
    
    enviar_paquete(paquete, socket_swap);
    eliminar_paquete(paquete);
    
    //  Esperar confirmación del SWAP
    int cod_op = recibir_operacion(socket_swap);
    
    //  Manejo de estados de la conexión
    if (cod_op == RESPUESTA_OK) {
        log_info(logger, "## Escritura exitosa: bloque %d", nro_bloque);
    } 
    else if (cod_op == RESPUESTA_ERROR) {
        log_error(logger, "## Error: El SWAP rechazó la escritura del bloque %d", nro_bloque);

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
}
void desuspender_proceso(int pid) {
    t_contexto* ctx = buscar_contexto(pid);
    if (!ctx) {
        log_error(logger, "No existe el proceso %d para desuspender", pid);
        return;
    }

    log_info(logger, "Des-suspendiendo proceso %d...", pid);

    for (int i = 0; i < list_size(ctx->tabla_segmentos); i++) {
        t_segmento_aux* seg = list_get(ctx->tabla_segmentos, i);
        
        if (seg->en_swap) {
            // Buscar nuevo lugar en RAM 
            // Esto devuelve la nueva dirección base
            int nueva_base = buscar_hueco_para_segmento(seg->limite); 
            
            if (nueva_base == -1) {
                log_error(logger, "ERROR: No hay espacio en RAM para el segmento %d del proceso %d", seg->id, pid);
                return; // O manejar una política de reemplazo si el sistema lo requiere
            }

            // Traer los datos del SWAP a la RAM
            // Usamos un buffer temporal del tamaño del segmento
            void* buffer = malloc(seg->limite);
            recibir_de_swap(seg, buffer);
            
            //  Escribir en la nueva dirección de la MS
            escribir_en_memoria(nueva_base, buffer, seg->limite);
            
            //  Actualizar metadatos del segmento
            seg->direccion_base = nueva_base;
            seg->en_swap = false;
            
            // seg->bloque_swap y seg->cantidad_bloques se limpian dentro de recibir_de_swap
            
            free(buffer);
            log_info(logger, "Segmento %d restaurado en dirección %d", seg->id, nueva_base);
        }
    }
    
    log_info(logger, "Proceso %d des-suspendido exitosamente", pid);
}-
#ifndef UTILS_H_
#define UTILS_H_

#include <commons/collections/list.h>
#include <commons/log.h> 
#include <commons/string.h>
#include <commons/config.h> // Para que reconozca t_config
#include <pthread.h>  
#include <stdint.h>
#include <commons/bitarray.h>
#include "../../utils/src/global_utils.h" // Aquí ya están los op_code y funciones de red


typedef struct {
    uint32_t direccion_base; 
    uint32_t tamanio;        
} t_hueco;

typedef struct {
    int id_segmento;
    int id_ms;
    int direccion_base;
    int limite;
    bool en_swap;         // ¿Está en el disco?
    int bloque_swap;      // ¿En qué bloque empieza en el SWAP?
    int cantidad_bloques; // ¿Cuántos bloques ocupa?
} t_segmento_aux;


typedef struct {
    int socket_fd;           // Socket de conexión física a esta Memory Stick
    uint32_t base_global;    // Dónde arranca en el mapa globalizado
    uint32_t tamanio; 
    char* port;
    char* ip;       // Cuánto espacio aporta
} t_memory_stick_nodo;


// VARIABLES GLOBALES
extern t_list* lista_huecos_libres;  
extern t_list* lista_contextos;      
extern t_list* lista_memory_sticks;  
extern t_list* lista_procesos;
extern uint32_t memoria_total_sistema;
extern int socket_swap;
extern int block_size_swap;
extern t_bitarray* bitmap_swap;
extern int total_bloques_swap;

extern pthread_mutex_t mutex_lista_libres;
extern pthread_mutex_t mutex_contextos;
extern pthread_mutex_t mutex_ms;
extern pthread_mutex_t mutex_procesos;

extern t_log* logger; 
extern t_config* config_km;
extern int socket_kernel_scheduler;



// Inicialización
void inicializar_utils(void);

// Gestión de Contextos y Procesos
t_contexto* crear_contexto(int pid);
void agregar_contexto(int pid);
t_contexto* buscar_contexto(int pid);
int buscar_indice_proceso(int pid);
int buscar_indice_contexto(int pid);

// Manejadores de Sockets / Protocolo KM
void manejar_crear_proceso(int socket_cliente);
void manejar_finalizar_proceso(int socket_cliente);
void manejar_pedido_instruccion_cpu(int socket_cliente);
void lectura_memoria(int socket_ks);
void escritura_memoria(int socket_ks);
void manejar_guardar_contexto(int socket_cliente);
void enviar_contexto_cpu(int socket_cpu, int pid);
void recibir_contexto_cpu(int socket_cpu);

// Gestión de Memory Sticks, Segmentación y Compactación
void conexion_memory_stick(int socket_ms);
t_memory_stick_nodo* buscar_ms_por_direccion_global(uint32_t dir_global);
void* leer_bytes_globales(uint32_t dir_global, uint32_t tamanio);
void escribir_bytes_globales(uint32_t dir_global, uint32_t tamanio, void* datos);
int calcular_espacio_libre_total(void);
t_hueco* seleccionar_hueco_segun_algoritmo(uint32_t tamanio_solicitado);
void ejecutar_compactacion_fisica_memory_stick(void);
void solicitar_y_ejecutar_compactacion(int socket_ks);
void creacion_segmento(int socket_cliente, int socket_ks, int pid, int id_segmento, uint32_t tamanio_segmento);
void eliminar_segmento(int pid, int id_segmento); // La que armamos recién
void manejar_caida_memory_stick(t_memory_stick_nodo* ms);

// SWAP
void suspender_proceso(int pid);
int desuspender_proceso(int pid);
int recibir_de_swap(t_segmento_aux* seg, void* buffer_destino);
void liberar_espacio_en_huecos(uint32_t direccion_base , uint32_t tamanio);
void enviar_a_swap (int nro_bloque , void* datos);
void liberar_bloques_swap(int nro_bloque, int cantidad);
#endif /* UTILS_H_ */

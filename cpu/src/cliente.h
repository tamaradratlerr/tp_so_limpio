#ifndef CLIENT_H_
#define CLIENT_H_

#include "utils.h"

typedef struct {
    int pid;
} t_proceso_ejec;

typedef struct {

    int conexion_kernel_memory;
    int conexion_kernel_scheduler;

    t_list* memory_sticks;

    char* ip_kernel_memory;
    char* puerto_kernel_memory;

    char* ip_kernel_scheduler;
    char* puerto_kernel_scheduler;
    char* puerto_kernel_scheduler_dispatch;
    char* puerto_kernel_scheduler_interrupt;

    char* ip_memory_stick;
    char* puerto_memory_stick;

} t_cpu_sockets;

/*----- VARIABLES -----*/

t_config* config                        = NULL;
t_log* logger                           = NULL;
t_contexto* contexto_actual             = NULL;
t_instruccion* instruccion_decodificada = NULL;
t_cpu_sockets* sockets                  = NULL;
t_proceso_ejec* proceso_en_ejecucion    = NULL;

char* identificador                     = NULL; 

int control_loop00                      = 0;
int control_loop                        = 0;


/*----- EXTERN -----*/
extern t_config* config; 
extern t_log* logger;
extern t_contexto* contexto_actual;
extern t_instruccion* instruccion_decodificada;
extern t_cpu_sockets* sockets;
extern t_proceso_ejec* proceso_en_ejecucion;

extern char* identificador; 

extern int control_loop00;
extern int control_loop;



/*------ PROTOTIPOS DE FUNCIONES ------*/

//--- Funciones Administrativas ---//
t_log* iniciar_logger(t_log_level log_level);
int iniciar_log_config (int archivo_config, char* identificador);
void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets* sockets);

//--- Conexiones ---//
int conexion_kernel_memory(t_config* config, t_log* logger, module_name module);
int conexion_kernelS(t_config* config, t_log* logger, module_name module);

//--- Ciclo de Instruccion ---//
char* fetch(t_cpu_sockets* sockets);
int decode(char* instruccion_raw);
void execute();
int interrupt();

//--- Funciones Complementarias para Ciclo de Instruccion ---//
t_contexto* recibir_contexto(int socket_km);
void liberar_instruccion(t_instruccion* instruccion);
void limpiar_contexto_actual();
void gestionar_desalojo_por_syscall(char* valor, op_code tipo_operacion);
void enviar_contexto_a_kernel_memory(void);
t_instruccion_code identificar_codigo(char* token);
void* obtener_registro(char* nombre);
bool es_registro_32bits(char* nombre);


//--- Funciones de Execute por Instruccion ---//
void ejecutar_noop(t_instruccion* instr);
void ejecutar_set(t_instruccion* instr);
void ejecutar_mov_in(t_instruccion* instr);
void ejecutar_mov_out(t_instruccion* instr);
void ejecutar_sum(t_instruccion* instr);
void ejecutar_sub(t_instruccion* instr);
void ejecutar_jnz(t_instruccion* instr);
void ejecutar_copy_mem(t_instruccion* instr);
void ejecutar_mutex_create(t_instruccion* instr);
void ejecutar_mutex_lock(t_instruccion* instr);
void ejecutar_mutex_unlock(t_instruccion* instr);
void ejecutar_mem_alloc(t_instruccion* instr);
void ejecutar_mem_free(t_instruccion* instr);
void ejecutar_sleep(t_instruccion* instr);
void ejecutar_stdout(t_instruccion* instr);
void ejecutar_stdin(t_instruccion* instr);
void ejecutar_init_proc(t_instruccion* instr);
void ejecutar_exit();

//--- Funciones Auxiliares de Execute ---//
void crear_segmento(int id, int tamanio, int base);
void eliminar_segmento(int id);

//--- MMU ---//
t_mem_stick* buscar_memory_stick(uint32_t direccion_fisica);
uint32_t pedir_direccion_mmu(uint32_t dir_logica, int tamanio_solicitado);
uint32_t obtener_direccion_del_registro(char* reg);
uint32_t obtener_tamanio_del_registro(char* reg);
void* leer_de_memoria(uint32_t dir_fisica, int tamanio);
void escribir_en_memoria(uint32_t dir_fisica, void* buffer, int tamanio);

//--- Manejo de Memory Stick ---//
void recibir_memory_stick(int socket_ks);
bool comparar_base_memory_stick(void* a, void* b);
bool rango_ocupado(t_mem_stick* nuevo);

//--- Auxiliares ---//
bool tiene_mismo_id(void* elemento);

//MOCK
t_contexto* recibir_contexto_mock ();
char* fetch_mock(t_cpu_sockets* sockets);
void enviar_contexto_a_kernel_memory_mock();
void* leer_de_memoria_mock(uint32_t dir_fisica, int tamanio);
void escribir_en_memoria_mock(uint32_t dir_fisica, void* buffer, int tamanio);

#endif
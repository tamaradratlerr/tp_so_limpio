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

int control_loop0                       = 0;
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
// Funciones Administrativas
t_log* iniciar_logger(t_log_level log_level);
void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets* sockets);

// Conexiones
int conexion_kernel_memory(t_config* config, t_log* logger, module_name module);
int conexion_kernelS(t_config* config, t_log* logger, module_name module);
int conexion_memory_stick(t_config* config, t_log* logger, module_name module);

// Ciclo de Instrucción
// Corregir en cliente.h
char* fetch(t_cpu_sockets* sockets);
int decode(char* instruccion_raw);
void execute(void);
void interrupciones(void);

// Funciones Complementarias
t_instruccion_code identificar_codigo(char* token);
void* obtener_registro(char* nombre);
bool es_registro_32bits(char* nombre);
void limpiar_contexto_actual(void);

// Ejecución de Instrucciones
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
void ejecutar_exit(void);

// Helpers de Memoria y Contexto
void enviar_contexto_a_kernel_memory(void);
t_contexto* recibir_contexto(int socket_km);
void* leer_de_memoria(uint32_t dir_fisica, int tamanio);
void escribir_en_memoria(uint32_t dir_fisica, void* buffer, int tamanio);
void gestionar_desalojo_por_syscall(char* valor, op_code tipo_operacion);
uint32_t pedir_direccion_mmu(uint32_t dir_logica, int tamanio_solicitado);
void liberar_instruccion(t_instruccion* instruccion);
uint32_t obtener_tamanio_del_registro(char* reg);
uint32_t obtener_direccion_del_registro(char* reg);
int apagar(void);

// MMU y Memoria
int obtener_tam_max_segmento ();
int obtener_tam_segmento_del_pid(int pid, int num_segmento);
int consultar_base_segmento_al_kernel(int num_segmento);

//MOCK
t_contexto* recibir_contexto_mock ();
char* fetch_mock(t_cpu_sockets* sockets);
uint32_t obtener_direccion_del_registro_mock(char* reg);
void enviar_contexto_a_kernel_memory_mock();
void* leer_de_memoria_mock(uint32_t dir_fisica, int tamanio);
void escribir_en_memoria_mock(uint32_t dir_fisica, void* buffer, int tamanio);
uint32_t pedir_direccion_mmu_mock(uint32_t dir_logica, int tamanio_solicitado);

#endif
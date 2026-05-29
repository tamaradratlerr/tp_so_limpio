#ifndef CLIENT_H_
#define CLIENT_H_

#include "utils.h"


// 1. PRIMERO DEFINIMOS LAS ESTRUCTURAS
typedef struct {
    int pid;
} t_proceso_ejec;

typedef struct {
    int conexion_kernel_memory;
    int conexion_kernel_scheduler;
    int conexion_memory_stick;
} t_cpu_sockets;

// 2. LUEGO DECLARAMOS LAS VARIABLES COMO EXTERNAS
// Esto dice: "esta variable existe en otro lado (cliente.c), no la crees aquí"
extern t_config* config; 
extern t_log* logger;
extern t_contexto* contexto_actual;
extern t_instruccion* instruccion_decodificada;
extern t_cpu_sockets* sockets;
extern t_proceso_ejec* proceso_en_ejecucion;

extern int control_loop00;
extern int control_loop;

/*------ PROTOTIPOS DE FUNCIONES ------*/
// Funciones Administrativas
t_log* iniciar_logger(void);
t_config* iniciar_config(void);
void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets* sockets);

// Conexiones
int conexion_kernel_memory(t_config* config, t_log* logger, module_name module);
int conexion_kernelS(t_config* config, t_log* logger, module_name module);
int conexion_memory_stick(t_config* config, t_log* logger, module_name module);

// Ciclo de Instrucción
// Corregir en cliente.h
char* fetch(t_cpu_sockets* sockets);
void decode(char* instruccion_raw);
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
void* leer_de_memoria(uint32_t dir_fisica, int tamanio);
void escribir_en_memoria(uint32_t dir_fisica, void* buffer, int tamanio);
void gestionar_desalojo_por_syscall(char* valor, op_code tipo_operacion);
int pedir_direccion_mmu(int32_t dir_logica);

#endif
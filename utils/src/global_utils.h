#ifndef GLOBAL_UTILS_H_
#define GLOBAL_UTILS_H_

#define _POSIX_C_SOURCE 200809L
#include <commons/collections/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <commons/string.h>
#include <stdint.h>
#include <commons/log.h>
#include <errno.h>
#include <commons/collections/list.h>
#include <assert.h>
#include <commons/config.h>
#include <semaphore.h> 
#include <pthread.h>  

extern t_log* logger;
extern char* PUERTO; // O como sea que definas el puerto en tu config

typedef enum {
    
    KERNEL_SCHEDULER,
    KERNEL_MEMORY,
    CPU,
    MEMORY_STICK,
	SWAP,
    IO

} module_name;

typedef enum // Todos los Posibles intercambios de informacion con la CPU, IO y KM.
{
    // Handshake
    OK, //El Handsaheke funciono correctamente
    NOTOK, //El Handshake no funciono correctamente 

    // Generales
    MENSAJE, //Pasaje de STRING
    PAQUETE, //Pasaje de datos Varios

    // CPU
    NUEVA_CPU, //Se informa que hay una nueva CPU
    CPU_LIBRE, //Se informa que una CPU no tiene ningun PCB asociado
    NUEVA_MEMORY_STICK,
    DESCONEXION_MS, //TAMI HACER -> km le manda ks y ks a cpu

    // Extras para CPU
    FIN_PROCESO, //Se informa que el proceso que estaba en la CPU finalizo
    DESALOJO, //Se informa que se debe desalojar al proceso que esta en la CPU
    PCB_DATA, //Se envia informacion como un PCB (PID)
    TRADUCIR_DIRECCION,
    ERROR_MMU,
    ERROR_SEGMENTATION_FAULT,

    // Syscalls de la CPU (Kernel Scheduler)
    gl_MUTEX_CREATE,
    gl_MUTEX_LOCK,
    gl_MUTEX_UNLOCK, 
    gl_MEM_ALLOC,
    gl_MEM_FREE,
    gl_IO_SLEEP,
    gl_IO_STDOUT,
    gl_IO_STDIN,
    gl_INIT_PROC,
    gl_EXIT,
    
    // CPU con la Memory Stick
    FETCH_INSTRUCCION,
    LLEGO_INSTRUCCION,

    //Ciclos CPU (para CPU -> KM)
    FETCH,
    LEER_MEMORIA,
    ESCRIBIR_MEMORIA,
    CONTEXTO,

    //KM
    MEM_CORRUPT, //ERROR que al recirbirlo por parte del KS (viene del KM) debe terminar todos los procesos y apagarse
    ENVIAR_PROCESO,
    km_IO_STDOUT, //Devolucion del IO STDOUT por parte de la KM
    km_IO_STDIN, //Devolucion del IO STDIN por parte de la KM
    SOLICITUD_INSTRUCCION,
    km_GUARDAR_CONTEXTO,
    ks_INIT_PROC,
    ks_EXIT,
    NUEVO_KERNEL,
    OK_ESCRITURA,
    NUEVO_ESPACIO, //En caso de nuevo MEM.STICK. o Se libere espacio (KM => KS) 
    SUSPENDIDO, //Informa que el proximo PID esta en estado SUSPENDIDO

    // IO
    NUEVA_IO, //Se informa que hay una nueva IO
    IO_LIBRE, //Se informa que una cpu no tiene ningun PCB asociado
    IO_STDIN,
    IO_STDOUT,
    IO_SLEEP,
    IO_STDOUT_RETORNO, //Devolucion del IO STDOUT por parte de la IO
    IO_STDIN_RETORNO, //Devolucion del IO STDIN por parte de la IO
    IO_FINALIZADA,
    
    // COMPACTACION
    CPUS_DESALOJADAS_OK,// El Kernel le avisa a la Memoria que las CPUs están vacías
    COMPACTACION,       
    COMPACTACION_FINALIZADA,   // La Memoria le avisa al Kernel que ya terminó de mover los bytes

    // nueva memoria que viene de memory stick
    NUEVA_MEMORIA_ACUM,

    // SWAP
    HANDSHAKE_SWAP = 100,
    LECTURA_BLOQUE,
    ESCRITURA_BLOQUE,
    RESPUESTA_OK,
    RESPUESTA_ERROR,
    RESPUESTA_DATOS


} op_code;

typedef struct {
    uint32_t size; // Tamaño del payload
    uint32_t offset; // Desplazamiento dentro del payload
    void* stream; // Payload
} t_buffer;

typedef struct {
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;


typedef struct {
    int pid;
    
    
    uint32_t pc;
    // Registros de 8 bits (uint8_t)
    uint8_t ax, bx, cx, dx;
    // Registros de 32 bits (uint32_t)
    uint32_t eax, ebx, ecx, edx;
    uint32_t si, di;
    //tabla de segmentos
     t_list* tabla_segmentos; 
} t_contexto;

typedef struct {
    int id_segmento;
    int tamanio;
    int base;
} t_segmento;

typedef struct{
    uint32_t base;
    uint32_t tamanio;

    char* ip;
    char* puerto;

    int socket;
} t_mem_stick;

typedef struct{
    t_mem_stick* memory_stick;
    uint32_t direccion_local;
} t_direccion_memoria;

typedef struct{
    t_mem_stick* memory_stick;
    uint32_t direccion_local;
} t_direccion_fisica;


typedef struct {
    int pid;
    t_list* instrucciones;
} t_proceso;

typedef enum 
{
    NEW,
    RDY,
    RNN,
    BCK,
    S_BCK,
    S_RDY,
    EXT,
    NO_ESTADO
    
    //Faltan agregar los estados del CheckPoint 3
}estado;

//Tipo de dato que ingresa desde el kernel memory
typedef struct {
    int PID, prioridad, prioridad_original;
 
} t_infoProceso;

typedef struct 
{
    t_list* mutex_tomados;
    t_infoProceso data;
    estado estado_pcb;
    estado estado_anterior;
    int fd_cpu; //socket cpu para que se sepa en q cpu se esta ejecutando

}PCB;

//Estructura de dato que identifica CPUs
typedef struct{
    int fd;
    char* identificador; 
    bool enUso; // EN USO = TRUE --- LIBRE = FALSE
}t_CPU;
typedef struct {
    int id_memory_stick;      // A qué chip le tenemos que hablar
    uint32_t direccion_fisica; // Dirección física LOCAL dentro de ese chip
    uint32_t tamanio;         
    uint32_t offset_buffer;    // Desde qué posición de nuestro buffer de datos arranca este pedazo
} t_sub_peticion;





/***** Tipos de datos que procesara el IO *****/
/*----- STDIN -----*/
typedef struct {
    uint32_t direc;
    uint32_t length;
	char* input;
} t_io_stdin;

/*----- STDOUT -----*/
typedef struct {
    uint32_t length;
    char* info;
} t_io_stdout;

/*----- SLEEP -----*/
typedef struct {
    uint32_t time;
} t_io_sleep;

typedef struct {
	int pid;
	op_code io_op_code; // [STDIN,STDOUT,SLEEP]
	t_io_sleep sleep; //Estructura con info necesaria para ejecutar un sleep
	t_io_stdout iostdout; //Estructura con info necesaria para ejecutar un stdout
	t_io_stdin iostdin; //Estructura con info necesaria para ejecutar un stdin
	} espera_io;



/*-----     FUNCIONES     -----*/

/*-----     COMUNICACION CLIENTE - SERVIDOR     -----*/
t_config* iniciar_config(char* path_config); 

int crear_conexion(char *ip, char* puerto, t_log*, module_name module);

int crear_conexion_reintentando(char *ip, char* puerto, t_log* logger, module_name module);

const char* getModuleName(module_name module);

void enviar_mensaje(char* mensaje, int socket_cliente);

void enviar_buffer(void *buffer, int size, int socket_cliente);

void* serializar_paquete(t_paquete* paquete, int bytes);

t_paquete* crear_paquete(op_code codigo);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

void enviar_op_code (op_code code, int socket_cliente); 

void enviar_paquete(t_paquete* paquete, int socket_cliente);

void eliminar_paquete(t_paquete* paquete);

void liberar_conexion(int socket_cliente);

void* recibir_buffer(int* size, int socket_cliente);
void enviar_solo_buffer(t_buffer* buffer, int socket);
op_code recibir_op_code (int socket_cliente);

t_list* recibir_paquete(int);

void iterator(char* value, t_log* logger);

char* recibir_mensaje(int socket_cliente, t_log* logger);
int esperar_cliente(int socket_servidor, t_log* logger);
int iniciar_servidor(char* puerto, t_log* logger);

int recibir_pid(int socket_cliente);

int enviar_pid(int PCB_ID, int socket_cliente);

void log_opcode(t_log* logger, op_code codigo);

char* opcode_to_string(op_code codigo);

void enviar_int(int code_op, int socket_cliente);

int recibir_int(int socket_cliente);


#endif /* GLOBAL_UTILS_H_ */

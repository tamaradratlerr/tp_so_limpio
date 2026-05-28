#ifndef GLOBAL_UTILS_H_
#define GLOBAL_UTILS_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <commons/log.h>
#include <errno.h>
#include <commons/collections/list.h>
#include <assert.h>
#include <commons/config.h>
#include <semaphore.h> 
#include <pthread.h>  


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

    // Extras para CPU
    FIN_PROCESO, //Se informa que el proceso que estaba en la CPU finalizo
    DESALOJO, //Se informa que se debe desalojar al proceso que esta en la CPU
    PCB_DATA, //Se envia informacion como un PCB (PID)

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
    
    km_IO_STDOUT, //Devolucion del IO STDOUT por parte de la KM
    km_IO_STDIN, //Devolucion del IO STDIN por parte de la KM

    // IO
    NUEVA_IO, //Se informa que hay una nueva IO
    IO_LIBRE, //Se informa que una cpu no tiene ningun PCB asociado

    IO_STDOUT_RETORNO, //Devolucion del IO STDOUT por parte de la IO
    IO_STDIN_RETORNO, //Devolucion del IO STDIN por parte de la IO


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

//Tipo de dato que ingresa desde el kernel memory
typedef struct {
    int PID, PPID, UID;
} t_infoProceso;

typedef struct {
    int pid;
    uint32_t pc;
    // Registros de 8 bits (uint8_t)
    uint8_t ax, bx, cx, dx;
    // Registros de 32 bits (uint32_t)
    uint32_t eax, ebx, ecx, edx;
    uint32_t si, di;
    //tabla de segmentos
     t_list* tabla_segmentos; //AGREGAR EN LA CPU ESTE
} t_contexto;

typedef enum 
{
    NEW,
    RNN,
    RDY,
    BCK,
    EXT
    
    //Faltan agregar los estados del CheckPoint 3
}estado;

typedef struct 
{
    t_infoProceso data;
    estado estado_pcb;
    estado estado_anterior;
    int fd_cpu; //socket cpu para que se sepa en q cpu se esta ejecutando

}PCB;

//Estructura de dato que identifica CPUs
typedef struct{
    int fd; 
    bool enUso; // EN USO = TRUE --- LIBRE = FALSE
}CPU;





/***** Tipos de datos que procesara el IO *****/
/*----- STDIN -----*/
typedef struct
{
	uint32_t pid;
    uint32_t bytes_to_read;
} t_io_stdin_recv;
typedef struct
{
	uint32_t pid;
    uint32_t input_length;
	char* input;
} t_io_stdin_send;

/*----- STDOUT -----*/
typedef struct
{
	uint32_t pid;
    uint32_t nombre_length;
    char* nombre;
} t_io_stdout;

/*----- SLEEP -----*/
typedef struct {
    uint32_t pid;
    uint32_t time;
} t_io_sleep;


/*-----     FUNCIONES     -----*/

/*-----     COMUNICACION CLIENTE - SERVIDOR     -----*/
int crear_conexion(char *ip, char* puerto, t_log*, module_name module);

const char* getModuleName(module_name module);

void enviar_mensaje(char* mensaje, int socket_cliente);

void* serializar_paquete(t_paquete* paquete, int bytes);

t_paquete* crear_paquete(op_code codigo);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

void enviar_op_code (op_code code, int socket_cliente); 

void enviar_paquete(t_paquete* paquete, int socket_cliente);

void eliminar_paquete(t_paquete* paquete);

void liberar_conexion(int socket_cliente);

void* recibir_buffer(int* size, int socket_cliente);

op_code recibir_op_code (int socket_cliente);

t_list* recibir_paquete(int);

void iterator(char* value);

char* recibir_mensaje(int);

int esperar_cliente(int);

int iniciar_servidor(void);

int recibir_pid (int socket_cliente);

int enviar_pid(int PCB_ID, int socket_cliente);

#endif /* GLOBAL_UTILS_H_ */

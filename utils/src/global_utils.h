#define _POSIX_C_SOURCE 200112L

#ifndef GLOBAL_UTILS_H_
#define GLOBAL_UTILS_H_

// 1. Primero SIEMPRE los tipos de sistema y sockets básicos
#include <sys/types.h>
#include <sys/socket.h>

// 2. Después las librerías de red que dependen de lo anterior
#include <netdb.h>
#include <unistd.h>

// 3. El resto de librerías estándar y de las cátedras
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <commons/log.h>

typedef enum {
    
    KERNEL_SCHEDULER,
    KERNEL_MEMORY,
    CPU,
    MEMORY_STICK,
	SWAP,
    IO

} module_name;

typedef enum //Todos los Posibles intercambios de informacion con la CPU, IO y KM.
{
	//Funcionamiento basico
    OK,
    NOTOK,
    
    MENSAJE,
	PAQUETE, 

	//con la CPU
	NUEVA_CPU,
    CPU_LIBRE,
    FIN_PROCESO,
    DESALOJO,
    PCB,

    //syscalls de la CPU --- Descripcion de cada una esta en el TP.
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOK,
    MEM_ALLOC,
    MEM_FREE,
    INIT_PROC,
    EXIT,


    //con el KM
    MEM_CORRUPT, //cortar todo con esta

	//con la IO
	SLEEP, 
	STDIN,
	STDOUT

}op_code;

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



int crear_conexion(char *ip, char* puerto, t_log*, module_name module);
const char* getModuleName(module_name module);
void enviar_mensaje(char* mensaje, int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes);
t_paquete* crear_paquete(op_code codigo);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_op_code (op_code code, int socket_cliente); 

#endif /* GLOBAL_UTILS_H_ */

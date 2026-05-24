#ifndef SERVER_H_
#define SERVER_H_

#include "utils.h"
 

/*----------------------------------FUNCIONES------------------------------------------*/

/*-----                     GESTION DE NUEVOS CLIENTES                     -----*/

void* atender_nuevo_cliente(void* argumento);

/*-----                     CREACION Y DESTRUCCION DE LISTAS                     -----*/

listas_procesos* Iniciar_listas_procesos (); //Funcion que inicializa listasProcesos (Variable Global) (HEAP)

void terminar_listas_procesos (); //Funcion que libera la memoria de listasProcesos (HEAP)

void iniciar_listas_suple (); //Funcion que inicializa las listas de CPUs y IOs (Suplementarias)

void eliminar_listas_suple(); //Funcion que destruye las listas de CPUs y IOs (Suplmentarias)

/*-----                     GESTION DE LISTAS                     -----*/

void agregar_proceso_lista (PCB* pcb); //Funcion que agrega un PCB (estructura) a su lista correspondiente segun su ESTADO ACTUAL.

void eliminar_proceso_Lista (PCB* pcb ); //Funcion que elimina un PCB (estructura) de su lista correspondiente segun su ESTADO ANTERIOR.

void agregar_lista_ready (PCB* pcb); //Funcion que agrega un PCB a la lista de READYS a partir de un ALGORITMO de PLANIFICACION*/

void ready_FIFO(PCB* pcb_nuevo); //Funcion que a partir del ALGORITMO FIFO agrega un PCB a LISTA DE READYS ordenando por PRIORIDAD*/

/*-----                     GESTION DE PCBs                     -----*/

void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado); //Funcion que cambia el estado actual y estado anterior de un PCB

void enviar_pcb(int PCB_ID, int socket_cliente); //Funcion que manda PID a un cliente 

/*-----                     GESTION DE CPUs                     -----*/

void mandar_proceso_cpu(int socket_cliente);

void enviar_desalojo(int socket_cliente);

/*-----                     GESTION DE IOs                     -----*/

void nueva_io (void* arg);

void atender_io_sleep(t_list* lista);

void atender_io_stdin(t_list* lista);

void atender_io_stdout(t_list* lista);

void mandar_proceso_io(IO* interfaz);

void io_libre (void* arg);

/*-----                     GESTION DE HILOS                     -----*/

void* hilo_quantum(void* arg);

/*-----                     Funciones de SERVIDOR                     -----*/

void* hilo_quantum(void* arg);

/*-----                     GESTION DE OP_CODEs                     -----*/

    //OK, 
    //NOTOK,
    
    //MENSAJE,
	//PAQUETE, 

	//con la CPU
	
    //NUEVA_CPU,
    void nueva_cpu (void* arg);
    //CPU_LIBRE,
    void cpu_libre (void* arg);
     //FIN_PROCESO,
    void fin_proceso (void* arg);
    //DESALOJO,
    //PCB,

    //syscalls de la CPU --- Descripcion de cada una esta en el TP.
    
    //MUTEX_CREATE,
    //MUTEX_LOCK,
    //MUTEX_UNLOK,
    //MEM_ALLOC,
    //MEM_FREE,
    //INIT_PROC,
    //EXIT,


    //con el KM
    
    //MEM_CORRUPT, /*cortar todo con esta*/

	//con la IO

    //IO_LIBRE,
    void io_libre (void* arg);
	//SLEEP, 
	//STDIN,
	//STDOUT



#endif /* SERVER_H_ */

#ifndef CPU_H_
#define CPU_H_

#include "utils.h"

t_contexto* contexto_actual; //Variable global que almacena el valor de todos los registros
t_instruccion* instruccion_decodificada; //Variable global que alamcena la intruccion a ejecutar
t_cpu_sockets* sockets; //Variable global que almacena las direcciones de memoria
t_proceso_ejec* proceso_en_ejecucion;
int control_loop00;
int control_loop;

typedef struct {
    int pid;
}t_proceso_ejec;

typedef struct {
    int conexion_kernel_memory;
    int conexion_kernel_scheduler;
    int conexion_memory_stick;
} t_cpu_sockets;


/*----- FUNCIONES LOG Y CONFIG ------*/

t_log* iniciar_logger(void);
t_config* iniciar_config(void);

/*------ FUNCIONES GENERALES ------*/

int conexion_kernel_memory(t_config* config, t_log* logger, module_name module);
int conexion_kernelS(t_config* config, t_log* logger, module_name module);
int conexion_memory_stick(t_config* config, t_log* logger, module_name module);
void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets sockets);



#endif /* CPU_H_ */
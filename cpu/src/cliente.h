#ifndef CPU_H_
#define CPU_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>

#include "utils.h"
t_contexto* contexto_actual; 
t_instruccion* instruccion_decodificada; 
t_cpu_sockets* sockets;

typedef struct {
    int conexion_kernel_memory;
    int conexion_kernel_scheduler;
    int conexion_memory_stick;
} t_cpu_sockets;



t_log* iniciar_logger(void);
t_config* iniciar_config(void);


int conexion_kernel_memory(t_config* config, t_log* logger, module_name module);
int conexion_kernelS(t_config* config, t_log* logger, module_name module);
int conexion_memory_stick(t_config* config, t_log* logger, module_name module);
void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets sockets);
int recibir_pid (int socket_cliente);


#endif /* CPU_H_ */
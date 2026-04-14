#ifndef CPU_H_
#define CPU_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>

#include "utils.h"

// Estructura para agrupar las conexiones y simplificar el pasaje de parámetros
typedef struct {
    int conexion_kernel_memory;
    int conexion_kernel_scheduler;
    int conexion_kernelS_dispatch;
    int conexion_kernelS_interrupt;
    int conexion_memory_stick;
} t_cpu_sockets;

// --- Funciones Administrativas (TP0) ---
t_log* iniciar_logger(void);
t_config* iniciar_config(void);
void leer_consola(t_log* logger);
void paquete(int conexion);
void enviar_mensaje(char* mensaje, int socket_cliente)
void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets sockets);

// --- Conexiones ---
// Estas funciones retornan el file descriptor de cada conexión
int conexion_kernel_memory(t_config* config);
int conexion_kernelS(t_config* config);
int conexion_kernelS_dispatch(t_config* config);
int conexion_kernelS_interrupt(t_config* config);
int conexion_memory_stick(t_config* config);

// --- Ciclo de Instrucción ---
//todavia no lo piden asi que chau

#endif /* CPU_H_ */
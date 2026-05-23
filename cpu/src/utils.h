#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>
#include <utilsKS/utilsKS.h>
typedef enum
{
    MENSAJE,
    PAQUETE,

    // CPU con el Kernel Scheduler
    CONTEXTO_EJECUTAR,
    INTERRUPT,
    TERMINO_PROCESO,
    
    BLOQUEAR_PROCESO,
    ks_IO_STDOUT,
    ks_IO_STDIN,
    ks_INIT_PROC,
    ks_EXIT,


    // CPU con la Memory Stick
    FETCH_INSTRUCCION,
    LLEGO_INSTRUCCION,

} op_code;



typedef struct {
    int pid;
    uint32_t pc;
    // Registros de 8 bits (uint8_t)
    uint8_t ax, bx, cx, dx;
    // Registros de 32 bits (uint32_t)
    uint32_t eax, ebx, ecx, edx;
    uint32_t si, di;
} t_contexto;

typedef enum {
    NOOP, SET, SUM, SUB, JNZ, COPY_MEM, MOV_IN, MOV_OUT,
    MUTEX_CREATE, MUTEX_LOCK, MUTEX_UNLOCK, 
    MEM_ALLOC, MEM_FREE, SLEEP, 
    STDOUT, STDIN, INIT_PROC, EXIT
} t_instruccion_code;

typedef struct {
    t_instruccion_code codigo;
    char* params[3]; // en los ejemplos ponían como mucho 3 parámetros
    int cant_params;
} t_instruccion;

typedef enum{

    //solicitud para km
    SOLICITUD_INSTRUCCION,
    LEER_MEMORIA,
    ESCRIBIR_MEMORIA,
    km_GUARDAR_CONTEXTO,

    //con ks
    ks_BLOQUEAR_PROCESO,
    ks_SLEEP,
    ks_IO_STDOUT,
    ks_IO_STDIN,
    ks_INIT_PROC,
    ks_EXIT

}op_code;

#endif /* UTILS_H_ */
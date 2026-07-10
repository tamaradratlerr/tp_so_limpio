#ifndef CLIENT_H_
#define CLIENT_H_

#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>

#include "utils.h"

extern t_log* logger;
extern t_config* config;
extern t_memory_stick_global ms_globals;
extern pthread_mutex_t mutex_memoria;

void arrancar_cliente_km(void);
void* atender_kernel_memory(void* arg);

#endif
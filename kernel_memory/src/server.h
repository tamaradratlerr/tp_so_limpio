#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include "utils.h"

void iterator(char* value, t_log* logger);
void* atender_cliente_inicial(void* arg);
void enviar_contexto_cpu(int socket_cpu); // Agrega 'int'
void recibir_contexto_cpu(int socket_cpu); // Agrega 'int'
#endif /* SERVER_H_ */
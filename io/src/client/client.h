#ifndef CLIENT_H_
#define CLIENT_H_
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>

#include "utils.h"
#include <global_utils.h>

t_log* iniciar_logger(char* log_level, char* file, char* process_name, char* is_active_console);

t_config* iniciar_config(char* path);
void enviar_opcode(int fd, op_code codigo);
//void leer_consola(t_log*); *** No parece necesario ***

//void paquete(int);*** No parece necesario ***

void terminar_programa(int, t_log*, t_config*);

void validar_argumentos(int, char**);

extern t_log* logger;
#endif /* CLIENT_H_ */
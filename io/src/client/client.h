#ifndef CLIENT_H_
#define CLIENT_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>

#include "utils.h"


t_log* iniciar_logger(char* log_level);

t_config* iniciar_config(void);

//void leer_consola(t_log*); *** No parece necesario ***

//void paquete(int);*** No parece necesario ***

void terminar_programa(int, t_log*, t_config*);

#endif /* CLIENT_H_ */
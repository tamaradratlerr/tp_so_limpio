#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include "utils.h"


extern int delay_memoria;
extern pthread_mutex_t mutex_memoria;


void iterator(char* value);
void validar_argumentos(int, char**);
#endif /* SERVER_H_ */

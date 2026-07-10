#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<commons/config.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>
#include "../../utils/src/global_utils.h" 




void init_memory_stick(uint32_t);
void* atender_cliente(void*);
void free_all_globals(void);

void validar_argumentos(int argc, char** argv);
void escribir_en_bloque_memoria(uint32_t dir_fisica, void* datos_a_escribir, uint32_t tamanio);
void* leer_de_bloque_memoria(uint32_t dir_fisica, uint32_t tamanio);

#endif /* UTILS_H_ */
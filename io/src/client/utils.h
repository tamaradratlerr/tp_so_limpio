#ifndef UTILS_H_
#define UTILS_H_


#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>

#include<commons/collections/list.h>
#include <global_utils.h>
/* Creación de paquete segun tipo de io */
t_paquete* crear_paquete_io(op_code);

int atender_peticiones_del_KS(int, t_log*);

void agregar_a_paquete(t_paquete*, void*, int);

void* serializar_paquete(t_paquete*, int);

void enviar_mensaje(char*, int);

void enviar_paquete(t_paquete*, int);

t_list* recibir_paquete(int);

void eliminar_paquete(t_paquete*);

void liberar_conexion(int);

#endif /* UTILS_H_ */
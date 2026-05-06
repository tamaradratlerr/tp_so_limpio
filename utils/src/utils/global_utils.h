#ifndef GLOBAL_UTILS_H_
#define GLOBAL_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>

typedef enum
{
    KERNEL_SCHEDULER,
    KERNEL_MEMORY,
    CPU,
    MEMORY_STICK,
	SWAP,
    IO,

} module_name;

int crear_conexion(char *ip, char* puerto, t_log*, module_name);
const char* getModuleName(module_name);

#endif /* GLOBAL_UTILS_H_ */

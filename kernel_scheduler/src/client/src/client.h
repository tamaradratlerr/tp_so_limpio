#ifndef CLIENT_H_
#define CLIENT_H_
#include "utils.h"

t_config* config;



t_log* iniciar_logger(void);
t_config* iniciar_config(void);

void terminar_programa(int, t_log*, t_config*);

#endif /* CLIENT_H_ */

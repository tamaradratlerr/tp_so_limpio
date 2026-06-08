#ifndef SERVER_H_
#define SERVER_H_

#include "utils.h"

pthread_t hilo_quantum;

/*----- GESTION DE NUEVOS CLIENTES -----*/
void* atender_nuevo_cliente(void* fd);

/*----- CREACION Y DESTRUCCION DE LISTAS -----*/
t_listas_procesos* Iniciar_listas_procesos(void);
void terminar_listas_procesos(void);
void iniciar_listas_suple(void);
void eliminar_listas_suple(void);

/*----- GESTION DE LISTAS -----*/
int agregar_proceso_lista(PCB* pcb);
op_code eliminar_proceso_Lista(PCB* pcb);
int agregar_lista_ready(PCB* pcb);
int ready_FIFO(PCB* pcb_nuevo);

/*----- GESTION DE PCBs -----*/
void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado);
PCB* buscar_pcb_por_pid(uint32_t pid_recibido);
PCB* encontrar_pcb_rnn_por_pid(int pid);

/*----- GESTION DE CPUs -----*/
void mandar_proceso_cpu(int socket_cliente);
void* control_hilo_quantum(void* arg);
bool es_la_cpu_buscada(void* elemento, void* contexto);
void enviar_desalojo(int socket_cliente);

/*----- GESTION DE OP_CODEs -----*/
void nueva_cpu(int cliente_fd);
void cpu_libre(int cliente_fd);
void fin_proceso(int cliente_fd);
void deslojarTodasCpus(void);
void mutex_create(int socket_cliente);
void mutex_lock(int socket_cliente);
void mutex_unlock(int socket_cliente);
void mem_alloc(int socket_cliente);
void mem_free(int socket_cliente);
void init_proc(int socket_cliente);
void exit_proceso(int socket_cpu);

/*----- GESTION DE IO -----*/
void io_sleep(int socket_cpu);
void rta_io_sleep(int socket_io);
void nueva_io(int cliente_fd);
void io_stdin(int socket_cpu);
void rta_io_stdin (int socket_io);
void io_stdout(int cpu_socket);
void rta_io_stdout(int socket_io);
void io_libre(int fd);

/*----- CON EL KERNEL MEMORY -----*/
void mem_corrupt(void);

/*----- AUXILIARES -----*/
void enviar_proceso_finalizar_KM(int pid);
void enviar_proceso_KM(uint32_t pid, op_code opCode);
bool es_el_mutex_buscado(void* elemento, void* contexto);




#endif /* SERVER_H_ */

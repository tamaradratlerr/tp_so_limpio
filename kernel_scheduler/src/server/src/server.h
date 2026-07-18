#ifndef SERVER_H_
#define SERVER_H_

#include "client.h"
#include "utils.h"

pthread_t hilo_quantum;

/*----- GESTION DE NUEVOS CLIENTES -----*/

void* atender_nuevo_cliente(void* fd);


/*----- GESTION DE PCBs -----*/
PCB* buscar_pcb_por_pid(int pid_recibido);
PCB* encontrar_pcb_rnn_por_pid(int pid);

/*----- GESTION DE CPUs -----*/

void mandar_proceso_cpu(int socket_cliente);
bool es_la_cpu_buscada(void* elemento, void* contexto);

/*----- ALGORTIMOS DE PLANIFICACION -----*/

void* control_hilo_quantum(void* arg);
PCB* obtener_siguiente_proceso();
int ready_FIFO(PCB* pcb_nuevo);
int ready_CMN(PCB* pcb_nuevo );
bool usa_quantum (PCB* pcb);
void mediano_plazo_bck(PCB* pcb);
void mediano_plazo_rdy (PCB* pcb);
void* hilo_reintentar_desuspension(void* arg);


/*----- GESTION DE IOs -----*/

bool es_la_io_buscada (void* elemento, void* contexto);

/*----- OP_CODEs -----*/

/*--- CPU ---*/

void nueva_cpu(int cliente_fd);
void cpu_libre(int cliente_fd);
void desalojo (int socket_cliente);

/*--- IO ---*/

void nueva_io(int cliente_fd);
void io_libre(int fd);
void io_finalizada(int io_socket);
/*--- KM ---*/

void mem_corrupt(int fd);
void compactacion (int socket_cliente, int pid_trigger);
bool hay_otro_proceso_ejecutando(int pid_trigger);
void recibir_nueva_memory_stick(int socket_km);
void nuevo_espacio();
/*----- HERENCIA -----*/

void actualizar_herencia(mutex_cpu* mutex);
void actualizar_prioridad_pcb(PCB* pcb, int nueva_prioridad);
void recalcular_prioridad(PCB* pcb);

/*----- AUXILIARES -----*/

void enviar_memory_stick_a_cpus(t_mem_stick* ms);
void enviar_proceso_finalizar_KM(int pid);
void enviar_proceso_KM(uint32_t pid, op_code opCode);
bool es_el_mutex_buscado(void* elemento, void* contexto);
espera_io* encontrar_pid_io_bck (int pid);
bool tiene_pid(void* elemento);
bool existe_pcb_con_pid(t_list* lista, int pid);
PCB* sacar_pcb_por_pid(t_list* lista, int pid) ;
void loguear_lista(t_list* lista, t_log* logger);

/*----- MOCKs -----*/

void enviar_proceso_finalizar_KM_mock (int pid);
void data_io_stdout_mock(espera_io* io_pcb, PCB* pcb, uint32_t tam);
void prueba_mediano_plazo_mock();
void prueba_largo_plazo_mock();

void prueba_compactacion_mock(void);
/*----- SYSCALLs CPU -----*/

void mutex_create(int socket_cliente);
void mutex_lock(int socket_cliente);
void mutex_unlock(int socket_cliente);
void mem_alloc(int socket_cliente);
void mem_free(int socket_cliente);
void init_proc(int socket_cliente);
void exit_proceso(int socket_cpu);
void io_sleep(int socket_cpu);
void rta_io_sleep(int socket_io);
void io_stdin(int socket_cpu);
void rta_io_stdin (int socket_io);
void io_stdout(int cpu_socket);
void rta_io_stdout(int socket_io);
void verificar_desalojo_por_prioridad(PCB* pcb_nuevo);
void loguear_lista_suplementaria(char* tipo_lista, t_log* logger);


#endif /* SERVER_H_ */

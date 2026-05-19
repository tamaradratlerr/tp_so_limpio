#include "utilsKS.h"

PCB* iniciar_pcb (int PID, int PPID, int UID){

	PCB* nuevo_pcb = malloc(sizeof(PCB));
	nuevo_pcb->data.PID = PID;
	nuevo_pcb->data.PPID = PPID;
	nuevo_pcb->data.UID = UID;
	nuevo_pcb->estado_pcb = NEW;
	nuevo_pcb->estado_anterior = NULL;

	return nuevo_pcb;
}

void terminar_pcb (PCB* pcb){
	free(pcb);

	return 0;
}
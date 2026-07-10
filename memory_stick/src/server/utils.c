#include "utils.h"

extern t_log* logger;
extern t_memory_stick_global ms_globals;

void init_memory_stick(uint32_t tamanio)
{
    ms_globals.tamanio = tamanio;
    ms_globals.memoria = malloc(tamanio);

    if (ms_globals.memoria == NULL) {
        log_error(logger, "ERROR: No se pudo reservar %u bytes de memoria", tamanio);
        abort();
    }

    memset(ms_globals.memoria, 0, tamanio);

    log_info(logger, "Memory Stick inicializado: %u bytes reservados", tamanio);

    ms_globals.cpus_conectadas = list_create();
}

void free_all_globals(void)
{
    free(ms_globals.memoria);
    list_destroy(ms_globals.cpus_conectadas);
}


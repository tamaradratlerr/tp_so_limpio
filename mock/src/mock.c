/*
 * cliente_mock.c
 * -------------------------------------------------------------
 * Cliente de prueba (mock) para el Kernel Scheduler.
 * Simula ser el módulo que dispara la syscall INIT_PROC, con
 * valores hardcodeados, para poder testear init_proc() del
 * lado del Kernel sin necesitar el módulo real.
 *
 * IMPORTANTE:
 * El formato de serialización de paquete (t_paquete / buffer)
 * de este archivo es el "clásico" de la cátedra (opcode + buffer
 * con pares [tamaño][dato]). Si tu protocolo.h / paquete.c real
 * difiere, avisame y lo ajusto para que matchee exacto con lo
 * que espera recibir_paquete() del Kernel.
 *
 * Uso:
 *   ./cliente_mock [ip] [puerto]
 *   (por defecto: 127.0.0.1 8000)
 * -------------------------------------------------------------
 */

 #include "mock.h"
 #include "../../utils/src/global_utils.h"

/* ============== CONEXIÓN ============== */

/* ============== MAIN ============== */

int main(int argc, char* argv[]) {
    char* ip     = (argc > 1) ? argv[1] : "127.0.0.1";
    char* puerto = (argc > 2) ? argv[2] : "8000"; 

    /* ---- Valores hardcodeados para la prueba de INIT_PROC ---- */
    char* path    = "/home/utnso/proceso_prueba.txt";
    int   prioridad = 3;
    int   pid       = 1; /* debe existir un PCB con este pid en el Kernel */

    int socket_kernel = crear_conexion(ip, puerto);
    printf("[MOCK] Conectado al Kernel Scheduler (%s:%s)\n", ip, puerto);

    /* 1) Paquete INIT_PROC: path + prioridad */
    t_paquete* paquete = crear_paquete(INIT_PROC);
    agregar_a_paquete(paquete, path, strlen(path) + 1);
    agregar_a_paquete(paquete, &prioridad, sizeof(int));
    enviar_paquete(paquete, socket_kernel);
    eliminar_paquete(paquete);
    printf("[MOCK] Enviado paquete INIT_PROC -> path: \"%s\" | prioridad: %d\n", path, prioridad);

    /* 2) PID suelto (lo que consume recibir_pid) */
    enviar_pid(socket_kernel, pid);
    printf("[MOCK] Enviado PID: %d\n", pid);

    /* 3) Espero la respuesta del Kernel (enviar_op_code(OK,...)) */
    int respuesta = recibir_op_code(socket_kernel);
    if (respuesta == OK) {
        printf("[MOCK] Kernel respondió OK. INIT_PROC procesado correctamente.\n");
    } else if (respuesta == -1) {
        printf("[MOCK] El Kernel cerró la conexión sin responder (revisar logs del server).\n");
    } else {
        printf("[MOCK] Kernel respondió con código inesperado: %d\n", respuesta);
    }

    close(socket_kernel);
    return 0;
}
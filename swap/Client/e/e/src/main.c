#include <utils/hello.h>

#include "config.h"
#include "logger.h"
#include "swap.h"
#include "conexion.h"
#include "operaciones.h"
#include <stdlib.h> // Para EXIT_FAILURE



int main(int argc, char* argv[]) {

    saludar("swap");
    
    //Validación de argumentos 

    if (argc < 2) {
        
        fprintf(stderr, "Error: Falta el path del archivo de configuracion");
        return EXIT_FAILURE;
    }

    //Cargar Configuración

    t_config_swap* config = cargar_config(argv[1]);
    if (config == NULL){
        fprintf(stderr, "Error: No se pudo cargar la configuracion\n");
        return EXIT_FAILURE; //chequeo
    } 

    // Iniciar Logger cuequear esto

    iniciar_logger(config->log_level);

   
    inicializar_swap(
        config->swap_file_path,
        config->swap_file_size,
        config->block_size
    );

    // Conexión a Kernel Memory
   
    int conexion = conectar_a_kernel(
        config->ip_kernel,
        config->puerto_kernel
    );

    if (conexion == -1) { //chequeo
        
        return EXIT_FAILURE; 
    }

    // conectar a Kernel Memory
    log_info(logger, "Conexion establecida con Kernel");

    // Handshake inicial
    enviar_info_swap(conexion,
                     config->block_size,
                     config->swap_file_size);


    atender_kernel(conexion);

    ///¿?
    cerrar_swap();
    destruir_config(config);
    destruir_logger();

    return 0;
}
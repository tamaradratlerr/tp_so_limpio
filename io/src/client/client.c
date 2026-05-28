#include "client.h"

int main(int argc, char** argv)
{
	validar_argumentos(argc, argv);

	int fd_conexion;
	char* clave;
	char* io_ip; 
	char* io_port; 
	char* log_level;
	char* log_file;
	char* log_process_name;
	char* log_is_active_console;
	char* io = argv[2];
	t_log* logger;

	/* Inicio configuración */
	t_config* io_config = iniciar_config(argv[1]);

	clave 					= config_get_string_value(io_config, "CLAVE");
	io_ip					= config_get_string_value(io_config, "IO_IP");		/* Ip del KS */
	io_port					= config_get_string_value(io_config, "IO_PORT");	/* Port del KS */
	log_level				= config_get_string_value(io_config, "LOG_LEVEL");
	log_file				= config_get_string_value(io_config, "LOG_FILE");
	log_process_name		= config_get_string_value(io_config, "LOG_PROCESS_NAME");
	log_is_active_console	= config_get_string_value(io_config, "LOG_IS_ACTIVE_CONSOLE");
	
	/* Inicio Logger */
	logger = iniciar_logger(log_level, log_file, log_process_name, log_is_active_console);
	log_info(logger, "Logger iniciado.");
	log_info(logger, "Ip del KS: %s", io_ip);
	log_info(logger, "Puerto KS: %s", io_port);


	/* Conexion a servidor */
	log_info(logger, "Iniciando conexión con el servidor...");
	fd_conexion = crear_conexion(io_ip, io_port, logger, KERNEL_SCHEDULER);

	/* Una vez conectados, quedamos a la espera de mensajes del KERNEL SCHEDULER.*/
	log_info(logger, "Esperando peticiones IO desde %s", getModuleName(KERNEL_SCHEDULER));
	
	int status = 0;

	while (status != -1)
    {
		/* Bucle principal de la IO */
        status = atender_peticiones_del_KS(fd_conexion, logger);
		op_code cod_op = recibir_operacion(fd_conexion);
		

		switch (cod_op) {
		case SLEEP:{
			t_paquete* paquete_io = recibir_paquete(fd_conexion);
			
			t_io_sleep* datos = (t_io_sleep*)paquete_io->buffer->stream;
			
			uint32_t pid = datos->pid;
			uint32_t mseg = datos->time;

			log_info(logger, "## PID: %u - Haciendo sleep por %u milisegundos.", pid, mseg);
			
			useconds_t useg = mseg * 1000;
			usleep(useg);
			
			enviar_mensaje("Finalizo OK", fd_conexion);
			enviar_opcode(fd_conexion, IO_LIBRE);
			eliminar_paquete(paquete_io);
			break;}

		case STDIN: {
			// recibir del ks la instrucción
			t_paquete* paquete_io = recibir_paquete(fd_conexion);
			t_io_stdin_recv* datos_stdin = (t_io_stdin_recv*)paquete_io->buffer->stream;
			
			uint32_t pid = datos_stdin->pid;
			uint32_t bytes_a_leer = datos_stdin -> bytes_to_read;
			uint32_t direccion_logica = datos_stdin -> direccion_logica;

			log_info(logger, "## PID: %u - Operación STDIN. Leyendo %u bytes.", pid, bytes_a_leer);
			
			// leer 
			char* buffer_usuario = malloc(bytes_a_leer);
			// Usamos read para garantizar el tamaño exacto sin basura
			read(STDIN_FILENO, buffer_usuario, bytes_a_leer);

			// devolver al ks los datos leídos para que él los reenvíe al km
			t_paquete* paquete_retorno = crear_paquete(IO_STDIN_RETORNO);
			agregar_a_paquete(paquete_retorno, &pid, sizeof(uint32_t));
			agregar_a_paquete(paquete_retorno, &direccion_logica, sizeof(uint32_t));
			agregar_a_paquete(paquete_retorno, &bytes_a_leer, sizeof(uint32_t));
			agregar_a_paquete(paquete_retorno, buffer_usuario, bytes_a_leer);
			
			enviar_paquete(paquete_retorno, fd_conexion); 
			enviar_opcode(fd_conexion, IO_LIBRE);
			free(buffer_usuario);
			eliminar_paquete(paquete_io);
			eliminar_paquete(paquete_retorno);
			break;}

		case STDOUT: {
			// recibir del ks
			t_paquete* paquete_io = recibir_paquete(fd_conexion);
			
			// Deserializar [pid, tam, datos]
			uint32_t pid, tam;
			void* stream = paquete_io->buffer->stream;
			memcpy(&pid, stream, sizeof(uint32_t));
			memcpy(&tam, stream + sizeof(uint32_t), sizeof(uint32_t));
			
			char* datos_a_imprimir = malloc(tam + 1);
			memcpy(datos_a_imprimir, stream + (sizeof(uint32_t) * 2), tam);
			datos_a_imprimir[tam] = '\0'; // Asegurar fin de cadena

			// imprimir en consola
			log_info(logger, "## PID: %u - Operación STDOUT. Imprimiendo: %s", pid, datos_a_imprimir);
			printf("%s\n", datos_a_imprimir);

			// notificar al ks que terminó
			t_paquete* paquete_fin = crear_paquete(IO_STDOUT_RETORNO);
			agregar_a_paquete(paquete_fin, &pid, sizeof(uint32_t));
			enviar_paquete(paquete_fin, fd_conexion);
			enviar_opcode(fd_conexion, IO_LIBRE);
			free(datos_a_imprimir);
			eliminar_paquete(paquete_io);
			eliminar_paquete(paquete_fin);
			break;
		}


		case -1:

			log_error(logger, "El cliente se desconectó");
			close(fd_conexion);
			return NULL;

		default:
			log_warning(logger,"IO desconocida.");
			
			break;
	}

	}
	
	
	terminar_programa(fd_conexion, logger, io_config);

	return 0;
}

/*
 ****************
 *	Funciones	*
 ****************
*/

t_config* iniciar_config(char* path)
{
	t_config* nuevo_config;

	nuevo_config = config_create(path);

    if (nuevo_config == NULL) {
        printf("¡No se pudo crear el config!\n");
        abort();
    }
	return nuevo_config;
}

t_log* iniciar_logger(char *log_level, char* file, char* process_name, char* is_active_console) //Ingreso valor del t_log_level para configurar la salida del mismo
{
	t_log* nuevo_logger;

	char *file = file;
	char *process_name = process_name;
	bool is_active_console = is_active_console;
	t_log_level level;

	//Condicional que me permite modificar el nivel del log a partir de lo recibido en el .config//
	if (strcmp(log_level, "INFO") == 0)
	{
		level = LOG_LEVEL_INFO;
	}
	else
	{
		level = LOG_LEVEL_DEBUG;
	}
	
	nuevo_logger = log_create(file, process_name, is_active_console, level);
	
	return nuevo_logger;
}


void terminar_programa(int conexion, t_log* logger, t_config* config) //libera la memoria del logger y el config
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}

void validar_argumentos(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Error, faltan argumentos. \nUso: %s <arg1> <arg2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

void enviar_opcode(int fd, op_code codigo) {
    send(fd, &codigo, sizeof(op_code), 0);
}
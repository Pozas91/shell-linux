/**
*	Datos Identificativos:
* Nombre: 		Nicolás
* Apellidos: 	Pozas García
* DNI:				79026446X
**/

/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell
	(then type ^D to exit program)

**/

/** remember to compile with module job_control.c **/
#include "job_control.h"

/** 256 chars per line, per command, should be enough. **/
#define MAX_LINE 256

/** Lista para almacenar los trabajos lanzados "struct job_" **/
job* lista_trabajos;

int length(char** array) {
	int count = 0;
	while(array[count]) {
		count++;
	}
	return count;
}

/** Manejador para la señal SIGCHLD **/
void manejador(int senal) {
	int i;

	// Recorremos la lista
	for(i = list_size(lista_trabajos); i >= 1; i--) {

		// Extraemos un trabajo
		job *aux = get_item_bypos(lista_trabajos, i);

		// Si no es de foreground, es decir BACKGROUND O STOPPED
		if(aux->state != FOREGROUND) {
			int status;

			// Vemos si ha cambiado su estado sin bloquearnos (Wait no bloqueante)
			// es totalmente necesario WNOHANG para no quedarnos bloqueados
			pid_t wpid = waitpid(aux->pgid, &status, WNOHANG | WUNTRACED);

			// Si es el caso
			if(wpid == aux->pgid) {
				int info;

				// Vemos su nuevo estado y damos el mensaje
				enum status st = analyze_status(status, &info);
				printf("Background pid: %d, command: %s, %s, info: %d \n", wpid, aux->command, status_strings[st], info);

				// Si se ha suspendido
				if(st == SUSPENDED) {
					// Actualizamos su estado en la lista
					aux->state = STOPPED;

				// Si no, es que ha terminado y por tanto lo eliminamos de la lista
				} else {
					// Lo eliminamos de la lista
					delete_job(lista_trabajos, aux);
				}
			}
		}
	}
}

// -----------------------------------------------------------------------
//														COMANDO FG
// -----------------------------------------------------------------------

void fgCommand(int index) {
	int size = list_size(lista_trabajos);	// Guardamos el tamaño de la lista

	if(empty_list(lista_trabajos)) {
		printf("La lista está vacía\n");
	} else if(index > size || index <= 0) {
		printf("El argumento indicado está fuera del rango de la lista\n");
		printf("La lista sólo contiene {%d} elementos\n", size);
	} else {

		// Extraemos el trabajo de la lista de trabajos
		job *trabajo = get_item_bypos(lista_trabajos, index);

		// Información sobre el proceso actual
		printf("Tarea: %d, pid: %d, comando: %s, estado anterior: %s. \n", index, trabajo->pgid, trabajo->command, state_strings[trabajo->state]);

		// Si la tarea está parada es que la hemos suspendido
		if(trabajo->state == STOPPED) {
			printf("Reanudando la tarea... \n");
			killpg(trabajo->pgid, SIGCONT);		// Enviamos al proceso la señal para reanudarlo
		}

		if(trabajo->state != FOREGROUND) {
			int status;
			int pgid;
			int info;
			pgid = trabajo->pgid;
			trabajo->state = FOREGROUND;

			printf("Tarea: %d, pid: %d, comando: %s, estado actual: %s. \n", index, trabajo->pgid, trabajo->command, state_strings[trabajo->state]);
			printf("Asignándole el terminal...\n");

			set_terminal(pgid);
			// Esperamos que el proceso siga haciendo su ejecución en primer plano
			pid_t wpid = waitpid(pgid, &status, WUNTRACED);

			// Vemos su nuevo estado y damos el mensaje
			enum status st = analyze_status(status, &info);
			printf("Background pid: %d, command: %s, %s, info: %d \n", wpid, trabajo->command, status_strings[st], info);

			block_SIGCHLD();

			if(st == SUSPENDED) {
				// Actualizamos su estado en la lista
				trabajo->state = STOPPED;
			// Si no, es que ha terminado y por tanto lo eliminamos de la lista
			} else {
				// Lo eliminamos de la lista
				delete_job(lista_trabajos, trabajo);
			}

			unblock_SIGCHLD();

			// Le devolvemos al terminal al proceso principal
			set_terminal(getpid());
		}
	}
}

// -----------------------------------------------------------------------
//														COMANDO BG
// -----------------------------------------------------------------------

void bgCommand(int index) {
	int size = list_size(lista_trabajos); // Guardamos el tamaño de la lista

	if(empty_list(lista_trabajos)) {
		printf("La lista está vacía\n");
	} else if(index > size || index < 0) {
		printf("El argumento indicado está fuera del rango de la lista \n");
		printf("La lista sólo contiene {%d} elementos \n", size);
	} else {
		// Extraemos el trabajo de la lista de trabajos
		job *trabajo = get_item_bypos(lista_trabajos, index);

		// Si la tarea está parada es que la hemos suspendido
		if(trabajo->state == STOPPED) {
			killpg(trabajo->pgid, SIGCONT);		// Enviamos al proceso la señal para reanudarlo
			trabajo->state = BACKGROUND;			// Cambiamos el estado en la lista
			printf("Tarea: {%d}, pid: %d, comando: %s, info: estaba suspendida, su nuevo estado es %s. \n", index, trabajo->pgid, trabajo->command, state_strings[trabajo->state]);
		} else {
			printf("Tarea: {%d}, pid: %d, comando: %s, info: no está suspendida, su estado es %s. \n", index, trabajo->pgid, trabajo->command, state_strings[trabajo->state]);
		}
	}
}

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

int main(void) {
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE / 2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */
	ignore_terminal_signals();	// Ignora las señales del terminal

	/** Asociamos el manejador a la señal SIGCHLD y creamos la lista de trabajos **/
	signal(SIGCHLD, manejador);
	lista_trabajos = new_list("Lista trabajos");

	/** Program terminates normally inside get_command() after ^D is typed **/
	while(1) {

		printf("COMMAND->");
		fflush(stdout);
		/** get next command **/
		get_command(inputBuffer, MAX_LINE, args, &background);

		// if empty command
		if(args[0] == NULL) {
			continue;
		}

		/** Comando interno CD **/
		if(strcmp(args[0], "cd") == 0 && args[1] != NULL) {
			chdir(args[1]);
			continue;
		}

		/** Comando interno JOBS (HECHO) **/
		if(strcmp(args[0], "jobs") == 0) {
			if(empty_list(lista_trabajos)) {
				printf("La lista de tareas está vacía\n");
			} else {
				print_job_list(lista_trabajos);
			}
			continue;
		}

		/** Comando interno BG (HECHO) **/
		if(strcmp(args[0], "bg") == 0) {
			if(args[1] == NULL) {
				printf("No le has pasado ningún argumento al comando \n");
			} else {
				int index = atoi(args[1]);   // Convertimos el indice pasado a un entero
				bgCommand(index);   // Invocamos al comando FG
			}
			continue;
		}

		/** Comando interno FG **/
		if(strcmp(args[0], "fg") == 0) {
			int index = 1;			// Por defecto a la primera tarea de la lista
			if(args[1] != NULL) {
				index = atoi(args[1]);	// Si no es nulo, lo aplicaremos al correspondiente
			}
			fgCommand(index);						// Invocamos al comando FG
			continue;
		}

		// the steps are:
		// (1) fork a child process using fork()

		pid_fork = fork();

		if(pid_fork < 0) {	// Caso de error
			printf("Error. The system wasn able to spawn a new process. Exiting...\n");
			exit(-1);

		} else if(pid_fork == 0) {	// Proceso hijo

			/** Nuevo identificador de grupo de procesos para el hijo, asociacion del terminal y señales a default **/
			pid_t mypid = getpid();
			new_process_group(mypid);	// Metemos al hijo en un nuevo grupo

			if(!background) {
				// Le asignamos el terminal
				set_terminal(mypid);
			}

			// Restauramos las señales que reciben del terminal
			restore_terminal_signals();

			// (2) the child process will invoke execvp()
			execvp(args[0], args);

			// Solo llegará aquí si no se ha producido el cambio de imagen
			printf("Error. Command not found: %s\n", args[0]);
			exit(-1);

		// Padre
		} else {

			/** Nuevo identificador de grupo de procesos para el hijo **/
			new_process_group(pid_fork);	// El padre se va a un nuevo grupo de trabajo

			// (3) if background == 0, the parent will wait, otherwise continue

			if(!background) {
				/**
				 * Wait con detección de suspension y recuperación del terminal
				 * Con el WUNTRACED se comprueba también si el hijo se suspende
				 **/
				set_terminal(pid_fork);	// Redundante
				pid_t wait_pid = waitpid(pid_fork, &status, WUNTRACED);
				set_terminal(getpid());

				/**
				 * pid_fork = waitpid(pid_fork, &status, 0);
				 * esta instruccion ya no es necesaria porque hacemos el wait_pid
				 **/
				int info;
				// Le pasamos la variable "status" que nos acaba de pasar el wait
				enum status st = analyze_status(status, &info);

				/**
				 * Si sale del bloqueo debido a la suspensión del estado
				 * lo metemos en la lista de tareas para poder llevar un
				 * seguimiento de los procesos suspendidos
				 **/
				if(st == SUSPENDED) {
					/** BLoqueamos la señal SIGCHLD, añadimos el trabajo a la lista (suspendido) y desbloqueamos la señal **/
					block_SIGCHLD();
					// Creamos un nuevo nodo para agregarlo a la lista
					job *aux = new_job(pid_fork, args[0], STOPPED);
					// Añadimos ese nodo a la lista creada anteriormente
					add_job(lista_trabajos, aux);
					unblock_SIGCHLD();
				}

				// (4) Shell shows a status message for processed command
				printf("Foreground pid: %d, command: %s, status: %s, info: %d \n", wait_pid, args[0], status_strings[st], info);

			} else {

				/** BLoqueamos la señal SIGCHLD, añadimos el trabajo a la lista(background) y desbloqueamos la señal **/
				block_SIGCHLD();
				// Creamos un nuevo nodo para agregarlo a la lista
				job *aux = new_job(pid_fork, args[0], BACKGROUND);
				// Añadimos ese nodo a la lista creada anteriormente
				add_job(lista_trabajos, aux);
				unblock_SIGCHLD();

				printf("Background job running... pid: %d, command: %s \n", pid_fork, args[0]);
			}
		}

		// (5) loop returns to get_commnad() function

	} // end while
}

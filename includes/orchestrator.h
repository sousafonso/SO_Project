#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H
#define MAX_TASKS 100
#define MAX_PROGS_PER_TASK 10
#define COMMAND_LENGTH 4096

typedef struct {
    char id[32];
    int estimated_time;
    char command[256];
    char args[256];
} Task;

/**
 * @brief Inicializa a comunicação com o cliente.
 * @param fifo_name Nome do FIFO.
 
*/
void setup_communication (const char *fifo_name);

/**
 * @brief Adiciona uma tarefa à fila de espera.
 * @param task Tarefa a ser adicionada.
*/
void enqueue_task (Task task);

/**
 * @brief Remove uma tarefa da fila de espera.
 * @return Tarefa removida.
*/
Task dequeue_task ();

/**
 * @brief Remove uma tarefa da lista de tarefas ativas.
 * @param index Índice da tarefa a ser removida.
*/
void remove_active_task (int index);

/**
 * @brief Executa uma tarefa.
 * @param task Tarefa a ser executada.
*/
void execute_task (Task task);

/**
 * @brief Monitora as tarefas ativas.
*/
void monitor_tasks ();

/**
 * @brief Lida com as requisições de tarefas.
 * @param fifo_name Nome do FIFO.
*/
void handle_task_requests (const char *fifo_name);

/**
 * @brief Inicia o servidor.
*/
int start_server ();


#endif /* ORCHESTRATOR_H */


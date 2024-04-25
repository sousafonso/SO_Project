#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H
#define MAX_TASKS 100
#define MAX_PROGS_PER_TASK 10
#define COMMAND_LENGTH 4096
#include <time.h>
#include <sys/types.h>

// Estrutura para armazenar o tempo de início de uma tarefa
struct TaskStartTime {
    char id[100];
    struct timeval start_time;
};

struct TaskStartTime task_start_times[MAX_TASKS];

typedef struct {
    char id[256];
    char command[1024];
    int estimated_time;
} Task;

typedef struct {
    Task task;
    pid_t pid;
    time_t start_time;
} ActiveTask;

typedef struct {
    Task task;
    time_t start_time;
    time_t end_time;
} CompletedTask;

void add_active_task(ActiveTask active_task);

void setup_communication(const char *fifo_name);

void save_state ();

void handle_status_command ();

void *handle_status_command_thread(void *arg);

void parse_client_request(const char *buffer, Task *task);

//void handle_client_request(int client_fd);

//void main_loop ();

/**
 * @brief Inicializa a comunicação com o cliente.
 * @param fifo_name Nome do FIFO.
 
*/
//void setup_communication (const char *fifo_name);

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
void execute_tasks();

/**
 * @brief Monitora as tarefas ativas.
*/
void monitor_tasks ();

/**
 * @brief Lida com as requisições de tarefas.
 * @param fifo_name Nome do FIFO.
*/
//void handle_task_requests (const char *fifo_name);

/**
 * @brief Inicia o servidor.
*/
int start_server ();

int main();

#endif /* ORCHESTRATOR_H */

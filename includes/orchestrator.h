#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#define MAX_PROGS_PER_TASK 10
#define COMMAND_LENGTH 4096

// Estrutura para armazenar o tempo de início de uma tarefa
struct TaskStartTime {
    char id[100];
    struct timeval start_time;
};


typedef struct {
    char id[256];
    char command[1024];
    int estimated_time;
} Task;

typedef struct {
    Task task;
    pid_t pid;
    struct timeval start_time; // Altere para struct timeval
} ActiveTask;

typedef struct {
    Task task;
    time_t start_time;
    time_t end_time;
} CompletedTask;

extern struct TaskStartTime *task_start_times;
extern struct CompletedTask *completed_tasks;
extern Task *waiting_queue;
extern ActiveTask *active_tasks;
extern pid_t *active_pids;

void handle_status_command(int fifo_fd);

void add_active_task(ActiveTask active_task);

void setup_communication(const char *fifo_name);

void parse_client_request(const char *buffer, Task *task);

void save_state();

void enqueue_task(Task task);

Task dequeue_task();

void remove_active_task(int index);

void execute_task(Task task);

void monitor_active_tasks();

void start_server(const char *output_folder, int parallel_tasks, const char *sched_policy);

int main();

#endif /* ORCHESTRATOR_H */


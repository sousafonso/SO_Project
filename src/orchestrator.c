#include "../includes/orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#define PIPE_NAME "orchestrator_fifo"
#define MAX_TASKS 100

CompletedTask completed_tasks[MAX_TASKS];
int completed_count = 0;

Task waiting_queue[MAX_TASKS];
int waiting_count = 0;

ActiveTask active_tasks[MAX_TASKS];
int active_count = 0;
pid_t active_pids[MAX_TASKS];

pthread_mutex_t lock;

void add_active_task(ActiveTask active_task) {
    pthread_mutex_lock(&lock);
    active_tasks[active_count] = active_task;
    active_pids[active_count] = active_task.pid;
    active_count++;
    pthread_mutex_unlock(&lock);
}

void setup_communication(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

void save_state() {
    FILE *file = fopen("state.txt", "w");
    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "Waiting tasks:\n");
    for (int i = 0; i < waiting_count; i++) {
        fprintf(file, "Task ID: %s, Command: %s\n", waiting_queue[i].id, waiting_queue[i].command);
    }

    fprintf(file, "\nActive tasks:\n");
    for (int i = 0; i < active_count; i++) {
        fprintf(file, "Task ID: %s, Command: %s, PID: %d\n", active_tasks[i].task.id, active_tasks[i].task.command, active_tasks[i].pid);
    }

    fclose(file);
}

void enqueue_task(Task task) {

    // Obter o tempo atual
    struct timeval now;
    gettimeofday(&now, NULL);

    // Armazenar o tempo de início da tarefa
    strcpy(task_start_times[waiting_count].id, task.id);
    task_start_times[waiting_count].start_time = now;

    if (waiting_count >= MAX_TASKS) {
        fprintf(stderr, "Fila de espera cheia. Tarefa %s não enfileirada.\n", task.id);
        return;
    }

    int i = 0;
    while (i < waiting_count && waiting_queue[i].estimated_time <= task.estimated_time) {
        i++;
    }

    for (int j = waiting_count; j > i; j--) {
        waiting_queue[j] = waiting_queue[j - 1];
    }

    waiting_queue[i] = task;
    waiting_count++;
    save_state();
}

Task dequeue_task() {
    if (waiting_count == 0) {
        fprintf(stderr, "Fila de espera vazia.\n");
        exit(EXIT_FAILURE);
    }

    Task task = waiting_queue[0];
    for (int i = 0; i < waiting_count - 1; i++) {
        waiting_queue[i] = waiting_queue[i + 1];
    }
    waiting_count--;
    save_state();
    return task;
}

void remove_active_task(int index) {
    // Obter o tempo atual
    struct timeval now;
    gettimeofday(&now, NULL);

    // Calcular o tempo de execução da tarefa
    struct timeval start_time = task_start_times[index].start_time;
    long execution_time = (now.tv_sec - start_time.tv_sec) * 1000 + (now.tv_usec - start_time.tv_usec) / 1000;

    // Registrar a tarefa em um arquivo
    FILE *file = fopen("task_log.txt", "a");
    if (file == NULL) {
        perror("fopen");
        return;
    }
    fprintf(file, "Task ID: %s, Execution time: %ld ms\n", active_tasks[index].task.id, execution_time);
    fclose(file);

    if (index < active_count - 1) {
        memmove(&active_tasks[index], &active_tasks[index + 1], (active_count - index - 1) * sizeof(Task));
        memmove(&active_pids[index], &active_pids[index + 1], (active_count - index - 1) * sizeof(pid_t));
    }
    active_count--;
    save_state();
}

void execute_tasks() {
    while (1) {
        // Verificar se algum processo filho terminou
        for (int i = 0; i < active_count; i++) {
            int status;
            pid_t pid = waitpid(active_pids[i], &status, WNOHANG);
            if (pid == -1) {
                perror("waitpid");
                return;
            } else if (pid == active_pids[i]) {
                // O processo filho terminou, remover a tarefa correspondente
                remove_active_task(i);
                break;
            }
        }

        // Se houver tarefas na fila de espera, iniciar a próxima tarefa
        if (waiting_count > 0) {
            Task task = dequeue_task();
            execute_task(task);
        }

        // Dormir por um curto período de tempo para evitar uso excessivo de CPU
        usleep(1000);
    }
}

void handle_status_command() {
    printf("Active tasks:\n");
    for (int i = 0; i < active_count; i++) {
        printf("Task %s: started at %ld, command: %s\n", active_tasks[i].task.id, active_tasks[i].start_time, active_tasks[i].task.command);
    }

    printf("Waiting tasks:\n");
    for (int i = 0; i < waiting_count; i++) {
        printf("Task %s: enqueued, command: %s\n", waiting_queue[i].id, waiting_queue[i].command);
    }

    printf("Completed tasks:\n");
    for (int i = 0; i < completed_count; i++) {
        printf("Task %s: started at %ld, ended at %ld, command: %s\n", completed_tasks[i].task.id, completed_tasks[i].start_time, completed_tasks[i].end_time, completed_tasks[i].task.command);
    }
}

void *handle_status_command_thread(void *arg) {
    int client_fd = *(int *)arg;
    handle_status_command();
    close(client_fd);
    return NULL;
}

// void main_loop() {
//     while (1) {
//         // Check for completed tasks
//         for (int i = 0; i < active_count; i++) {
//             int status;
//             pid_t pid = waitpid(active_tasks[i].pid, &status, WNOHANG);
//             if (pid > 0) {
//                 // Task completed
//                 remove_active_task(i);
//                 i--;  // Adjust index after removing task
//             } else if (pid < 0) {
//                 perror("waitpid");
//             }
//         }

//         // Start new tasks if there is room
//         while (active_count < MAX_TASKS && waiting_count > 0) {
//             Task task = dequeue_task();
//             execute_task(task);
//         }

//         sleep(1);  // Avoid busy waiting
//     }
// }

void parse_client_request(const char *buffer, Task *task) {
    char *token = strtok(buffer, " ");
    if (token == NULL) {
        fprintf(stderr, "Comando inválido.\n");
        return;
    }

    if (strcmp(token, "task") != 0) {
        fprintf(stderr, "Comando inválido.\n");
        return;
    }

    token = strtok(NULL, " ");
    if (token == NULL) {
        fprintf(stderr, "ID da tarefa não fornecido.\n");
        return;
    }

    strncpy(task->id, token, sizeof(task->id) - 1);

    token = strtok(NULL, "");
    if (token == NULL) {
        fprintf(stderr, "Comando da tarefa não fornecido.\n");
        return;
    }

    strncpy(task->command, token, sizeof(task->command) - 1);
    task->estimated_time = 0;
}

// void handle_client_request(int client_fd) {
//     char buffer[1024];
//     ssize_t num_read = read(client_fd, buffer, sizeof(buffer) - 1);
//     if (num_read <= 0) {
//         perror("read");
//         return;
//     }

//     buffer[num_read] = '\0';

//     if (strcmp(buffer, "status") == 0) {
//         pthread_t thread;
//         if (pthread_create(&thread, NULL, handle_status_command_thread, &client_fd) != 0) {
//             perror("pthread_create");
//             return;
//         }
//         pthread_detach(thread);
//     } else {
//         Task task;
//         parse_client_request(buffer, &task);
//         enqueue_task(task);
//     }
// }

/**
 * Ele percorre todas as tarefas ativas. Para cada tarefa, ele usa waitpid com a opção WNOHANG para 
 * verificar se a tarefa terminou. Se a tarefa terminou, ele registra o tempo de término, 
 * calcula a duração real da tarefa, imprime uma mensagem e remove a tarefa da lista de tarefas ativas.
 * 
 * Depois de verificar todas as tarefas ativas, ele verifica se há tarefas esperando para serem iniciadas
 *  e se há espaço para iniciar mais tarefas. Se houver, ele desenfileira a próxima tarefa e a inicia.
 * 
 * Finalmente, ele faz uma pausa de um segundo antes de começar o próximo ciclo.
*/
void monitor_tasks() {
    while (1) {
        for (int i = 0; i < active_count; ) {
            int status;
            pid_t result = waitpid(active_tasks[i].pid, &status, WNOHANG);
            if (result == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            } else if (result == 0) {
                i++;
            } else {
                time_t end_time = time(NULL);
                time_t duration = end_time - active_tasks[i].start_time;
                printf("Task %s finished. Real duration: %ld seconds.\n", active_tasks[i].task.id, duration);
                completed_tasks[completed_count++] = (CompletedTask){
                    .task = active_tasks[i].task,
                    .start_time = active_tasks[i].start_time,
                    .end_time = end_time
                };
                remove_active_task(i);
            }
        }

        while (waiting_count > 0 && active_count < MAX_TASKS) {
            Task next_task = dequeue_task();
            execute_task(next_task);
        }

        sleep(1);
    }
}

int start_server() {
    setup_communication(PIPE_NAME);

    printf ("Servidor iniciado.\n");

    int fifo_fd; // Declare fifo_fd variable
    char buffer[1024];
    while (1) {
        ssize_t num_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        if (num_read <= 0) {
            perror("read");
            continue;
        }

        buffer[num_read] = '\0';

        Task task;
        parse_client_request(buffer, &task);
        enqueue_task(task);
    }

    monitor_tasks();

    close(fifo_fd);
    return 0;
}

int main() {
    if (start_server() != 0) {
        fprintf(stderr, "Erro ao iniciar o servidor.\n");
        return 1;
    }

    return 0;
}

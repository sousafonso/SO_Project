#include "../includes/orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
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

void parse_client_request(const char *buffer, Task *task) {
    if (buffer == NULL || task == NULL) {
        fprintf(stderr, "Erro: buffer ou task não podem ser nulos.\n");
        return;
    }

    // Tokeniza o buffer para extrair o ID da tarefa e o comando
    char *token = strtok(buffer, " ");
    if (token == NULL) {
        fprintf(stderr, "Erro: comando inválido.\n");
        return;
    }

    if (strcmp(token, "task") != 0) {
        fprintf(stderr, "Erro: comando inválido.\n");
        return;
    }

    // Extrai o ID da tarefa
    token = strtok(NULL, " ");
    if (token == NULL) {
        fprintf(stderr, "Erro: ID da tarefa não fornecido.\n");
        return;
    }
    strncpy(task->id, token, sizeof(task->id) - 1);

    // Extrai o comando da tarefa
    token = strtok(NULL, "");
    if (token == NULL) {
        fprintf(stderr, "Erro: comando da tarefa não fornecido.\n");
        return;
    }
    strncpy(task->command, token, sizeof(task->command) - 1);

    // Defina um tempo estimado padrão (por exemplo, 0) ou ajuste conforme necessário
    task->estimated_time = 0;
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
    if (waiting_count >= MAX_TASKS) {
        fprintf(stderr, "Fila de espera cheia. Tarefa %s não enfileirada.\n", task.id);
        return;
    }

    waiting_queue[waiting_count++] = task;
    save_state();
}

Task dequeue_task() {
    if (waiting_count == 0) {
        fprintf(stderr, "Fila de espera vazia.\n");
        exit(EXIT_FAILURE);
    }

    Task task = waiting_queue[0];
    for (int i = 1; i < waiting_count; i++) {
        waiting_queue[i - 1] = waiting_queue[i];
    }
    waiting_count--;
    save_state();
    return task;
}

void remove_active_task(int index) {
    if (index < 0 || index >= active_count) {
        fprintf(stderr, "Índice de tarefa ativa inválido.\n");
        return;
    }

    // Obter o tempo atual
    struct timeval now;
    gettimeofday(&now, NULL);

    // Calcular o tempo de execução da tarefa
    long execution_time = (now.tv_sec - active_tasks[index].start_time.tv_sec) * 1000 +
                          (now.tv_usec - active_tasks[index].start_time.tv_usec) / 1000;

    // Registrar a tarefa em um arquivo
    FILE *file = fopen("task_log.txt", "a");
    if (file == NULL) {
        perror("fopen");
        return;
    }
    fprintf(file, "Task ID: %s, Execution time: %ld ms\n", active_tasks[index].task.id, execution_time);
    fclose(file);

    if (index < active_count - 1) {
        memmove(&active_tasks[index], &active_tasks[index + 1], (active_count - index - 1) * sizeof(ActiveTask));
        memmove(&active_pids[index], &active_pids[index + 1], (active_count - index - 1) * sizeof(pid_t));
    }
    active_count--;
    save_state();
}

void execute_task(Task task) {
    // Criar processo filho para executar a tarefa
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Processo filho: executar o comando da tarefa
        execlp("sh", "sh", "-c", task.command, NULL);
        perror("execlp");
        exit(EXIT_FAILURE); // Em caso de falha na execução do comando
    } else {
        // Processo pai: adicionar tarefa à lista de tarefas ativas
        ActiveTask active_task = {.task = task, .pid = pid, .start_time = {0}};
        gettimeofday(&active_task.start_time, NULL);
        add_active_task(active_task);
    }
}

void monitor_tasks() {
    while (1) {
        for (int i = 0; i < active_count; i++) {
            int status;
            pid_t pid = waitpid(active_pids[i], &status, WNOHANG);
            if (pid == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                // Processo filho terminou
                remove_active_task(i);
            }
        }

        while (waiting_count > 0 && active_count < MAX_TASKS) {
            Task next_task = dequeue_task();
            execute_task(next_task);
        }

        sleep(1); // Aguardar antes de verificar novamente
    }
}

int start_server() {
    setup_communication(PIPE_NAME);
    printf("Servidor iniciado.\n");

    int fifo_fd = open(PIPE_NAME, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    ssize_t num_read;

    while (1) {
        num_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        if (num_read == -1) {
            perror("read");
            continue;
        } else if (num_read == 0) {
            // Nenhum dado disponível para ler, continue para a próxima iteração
            continue;
        }

        buffer[num_read] = '\0'; // Terminar o buffer com nulo

        Task task;
        parse_client_request(buffer, &task);
        enqueue_task(task);
    }

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


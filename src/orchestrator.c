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

#define FIFO_NAME "orchestrator_fifo"
#define FIFO_PATH "/tmp/" FIFO_NAME
#define MAX_TASKS 100
#define MAX_COMMAND_SIZE 4096

CompletedTask completed_tasks[MAX_TASKS];
int completed_count = 0;

Task waiting_queue[MAX_TASKS];
int waiting_count = 0;

ActiveTask active_tasks[MAX_TASKS];
int active_count = 0;
pid_t active_pids[MAX_TASKS];

pthread_mutex_t lock;

void handle_status_command(int fifo_fd) {
    char status_message[4096] = "Tarefas em espera e ativas:\n";

    // Adicionar o status das tarefas em espera ao status_message
    strcat(status_message, "Tarefas em espera:\n");
    for (int i = 0; i < waiting_count; i++) {
        char task_info[256];
        snprintf(task_info, sizeof(task_info), "Task ID: %s, Command: %s\n", waiting_queue[i].id, waiting_queue[i].command);
        strcat(status_message, task_info);
    }

    // Adicionar o status das tarefas ativas ao status_message
    strcat(status_message, "\nTarefas ativas:\n");
    for (int i = 0; i < active_count; i++) {
        char task_info[256];
        snprintf(task_info, sizeof(task_info), "Task ID: %s, Command: %s, PID: %d\n", active_tasks[i].task.id, active_tasks[i].task.command, active_pids[i]);
        strcat(status_message, task_info);
    }

    // Envia a mensagem de status de volta para o cliente
    if (write(fifo_fd, status_message, strlen(status_message) + 1) == -1) {
        perror("write");
    }
}

void add_active_task(ActiveTask active_task) {
    pthread_mutex_lock(&lock);
    active_tasks[active_count] = active_task;
    active_pids[active_count] = active_task.pid;
    active_count++;
    pthread_mutex_unlock(&lock);
}

void setup_communication(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }
}

void parse_client_request(const char *buffer, Task *task) {
    if (buffer == NULL || task == NULL) {
        fprintf(stderr, "Erro: buffer ou task não podem ser nulos.\n");
        return;
    }

    char execute_keyword[8];
    char time_keyword[3];
    char command_buffer[300]; // Defina um tamanho adequado para o seu comando

    // Primeiro tenta ler a palavra "execute" e o tempo estimado
    int num_args_scanned = sscanf(buffer, "%7s %d %2s", execute_keyword, &task->estimated_time, time_keyword);
    if (num_args_scanned != 3 || strcmp(execute_keyword, "execute") != 0 || (strcmp(time_keyword, "-u") != 0 && strcmp(time_keyword, "-p") != 0)) {
        fprintf(stderr, "Erro: comando inválido ou tempo estimado ausente.\n");
        return;
    }

    // Após ler o tempo estimado, move o buffer para frente, além do tempo
    buffer = strchr(buffer, ' '); // Encontra o espaço após "execute"
    buffer = strchr(buffer + 1, ' '); // Encontra o espaço após o tempo estimado
    buffer = strchr(buffer + 1, ' '); // Encontra o espaço após "-u" ou "-p"
    if (!buffer) {
        fprintf(stderr, "Erro: formato de comando inválido.\n");
        return;
    }
    buffer++; // Avança para o primeiro caractere do comando

    // Agora, tenta ler o comando entre aspas
    if (sscanf(buffer, "\"%[^\"]\"", command_buffer) != 1) {
        fprintf(stderr, "Erro: comando ausente ou não entre aspas.\n");
        return;
    }

    // Copia o comando para a estrutura Task
    strncpy(task->command, command_buffer, sizeof(task->command) - 1);
    task->command[sizeof(task->command) - 1] = '\0'; // Garante o terminador nulo
}

// VERSÃO 2
// void parse_client_request(const char *buffer, Task *task) {
//     char execute_keyword[8];
//     char command_buffer[MAX_COMMAND_SIZE]; // Defina um tamanho adequado para o seu comando

//     // Primeiro tenta ler a palavra "execute" e o tempo estimado
//     int num_args_scanned = sscanf(buffer, "%7s %d", execute_keyword, &task->estimated_time);
//     if (num_args_scanned != 2 || strcmp(execute_keyword, "execute") != 0) {
//         fprintf(stderr, "Erro: comando inválido ou tempo estimado ausente.\n");
//         return;
//     }

//     // Após ler o tempo estimado, move o buffer para frente, além do tempo
//     buffer = strchr(buffer, ' '); // Encontra o espaço após "execute"
//     buffer = strchr(buffer + 1, ' '); // Encontra o espaço após o tempo estimado
//     if (!buffer) {
//         fprintf(stderr, "Erro: formato de comando inválido.\n");
//         return;
//     }
//     buffer++; // Avança para o primeiro caractere do comando

//     // Agora, tenta ler o comando entre aspas
//     if (sscanf(buffer, "\"%[^\"]\"", command_buffer) != 1) {
//         fprintf(stderr, "Erro: comando ausente ou não entre aspas.\n");
//         return;
//     }

//     // Copia o comando para a estrutura Task
//     strncpy(task->command, command_buffer, sizeof(task->command) - 1);
//     task->command[sizeof(task->command) - 1] = '\0'; // Garante o terminador nulo
// }

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

    // Registar a tarefa num arquivo
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

// int start_server() {
//     setup_communication(PIPE_NAME);
//     printf("Servidor iniciado.\n");

//     int fifo_fd = open(PIPE_NAME, O_RDONLY);
//     if (fifo_fd == -1) {
//         perror("open");
//         exit(EXIT_FAILURE);
//     }

//     char buffer[1024];
//     ssize_t num_read;

//     while (1) {
//         num_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
//         if (num_read == -1) {
//             perror("read");
//             continue;
//         } else if (num_read == 0) {
//             // Nenhum dado disponível para ler, continue para a próxima iteração
//             continue;
//         }

//         buffer[num_read] = '\0'; // Terminar o buffer com nulo

//         Task task;
//         parse_client_request(buffer, &task);
//         enqueue_task(task);
//     }

//     close(fifo_fd);
//     return 0;
// }

int start_server() {
    setup_communication(FIFO_PATH);
    printf("Servidor iniciado.\n");

    int fifo_fd = open(FIFO_PATH, O_RDONLY);
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
            continue; 
        }

        buffer[num_read] = '\0'; // Terminar o buffer com nulo

        if (strcmp(buffer, "status") == 0) {
            int write_fd = open(FIFO_PATH, O_WRONLY);
            if (write_fd == -1) {
                perror("open");
                continue;
            }
            handle_status_command(write_fd);
            close(write_fd);
        } else {
            Task task;
            parse_client_request(buffer, &task);
            enqueue_task(task);
        }
    }

    close(fifo_fd);
    return 0;
}

int main() {
    start_server();
    return 0;
}
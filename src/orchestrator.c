#include "orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define FIFO_NAME "orchestrator_fifo"
#define MAX_TASKS 100
#define MAX_PROGS_PER_TASK 10

typedef struct {
    int id;
    pid_t pid;
    char commands[MAX_PROGS_PER_TASK][4096];
    int num_commands;
    time_t start_time;
} Task;

Task tasks[MAX_TASKS];
int num_tasks = 0;
int parallel_tasks = 1; // Número padrão de tarefas em paralelo

void execute_task(char *commands) {
    int task_index = -1;
    for (int i = 0; i < num_tasks; ++i) {
        if (tasks[i].pid == -1) {
            task_index = i;
            break;
        }
    }

    if (task_index == -1 || num_tasks >= MAX_TASKS) {
        fprintf(stderr, "Limite de tarefas atingido\n");
        return;
    }

    int id = rand(); // Gerar um identificador único para a tarefa
    tasks[task_index].id = id;
    tasks[task_index].num_commands = 0;

    char *token = strtok(commands, "|");
    while (token != NULL && tasks[task_index].num_commands < MAX_PROGS_PER_TASK) {
        snprintf(tasks[task_index].commands[tasks[task_index].num_commands], sizeof(tasks[task_index].commands[tasks[task_index].num_commands]), "%s", token);
        token = strtok(NULL, "|");
        tasks[task_index].num_commands++;
    }

    tasks[task_index].pid = fork();
    if (tasks[task_index].pid == -1) {
        perror("Falha ao criar processo filho");
        return;
    }

    if (tasks[task_index].pid == 0) { // Processo filho
        time(&tasks[task_index].start_time);
        char filename[256];
        snprintf(filename, sizeof(filename), "%d.out", id);
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1) {
            perror("Falha ao abrir arquivo de saída");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < tasks[task_index].num_commands; ++i) {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("Falha ao criar pipe");
                exit(EXIT_FAILURE);
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("Falha ao criar processo filho");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) { // Processo filho
                if (i != 0) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                } else {
                    dup2(fd, STDOUT_FILENO);
                }

                if (i != tasks[task_index].num_commands - 1) {
                    close(pipefd[1]);
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);
                }

                execlp("/bin/sh", "sh", "-c", tasks[task_index].commands[i], NULL);
                perror("Falha ao executar o comando");
                exit(EXIT_FAILURE);
            } else {
                if (i != 0) {
                    close(pipefd[1]);
                }
                if (i != tasks[task_index].num_commands - 1) {
                    close(pipefd[0]);
                }
            }
        }

        close(fd);
        for (int i = 0; i < tasks[task_index].num_commands; ++i) {
            wait(NULL);
        }

        exit(EXIT_SUCCESS);
    }

    num_tasks++;
    printf("Tarefa com ID %d em execução\n", id);
}

void handle_status_request() {
    printf("Tarefas em execução:\n");
    for (int i = 0; i < num_tasks; ++i) {
        if (tasks[i].pid != -1) {
            printf("ID: %d | Comando: %s\n", tasks[i].id, tasks[i].command);
        }
    }
}

int orchestrator(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s output_folder parallel-tasks sched-policy\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *output_folder = argv[1];
    parallel_tasks = atoi(argv[2]);
    char *sched_policy = argv[3];

    umask(0);
    mkfifo(FIFO_NAME, 0666);

    int fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        perror("Falha ao abrir o pipe nomeado");
        exit(EXIT_FAILURE);
    }

    char buffer[4096];
    while (1) {
        if (read(fd, buffer, sizeof(buffer)) > 0) {
            char *token = strtok(buffer, " ");
            if (strcmp(token, "execute") == 0) {
                char *time_str = strtok(NULL, " ");
                char *command = strtok(NULL, "");
                execute_task(command);
            } else if (strcmp(token, "status") == 0) {
                handle_status_request();
            }
        }
    }

    close(fd);
    unlink(FIFO_NAME);
    exit(EXIT_SUCCESS);
}


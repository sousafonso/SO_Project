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

#define PIPE_NAME "orchestrator_fifo"
#define MAX_TASKS 100

Task waiting_queue[MAX_TASKS];
int waiting_count = 0;

Task active_tasks[MAX_TASKS];
int active_count = 0;
pid_t active_pids[MAX_TASKS];

void setup_communication(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

void enqueue_task(Task task) {
    if (waiting_count < MAX_TASKS) {
        waiting_queue[waiting_count++] = task;
    } else {
        fprintf(stderr, "Fila de espera cheia. Tarefa %s não enfileirada.\n", task.id);
    }
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
    return task;
}

void remove_active_task(int index) {
    if (index < active_count - 1) {
        memmove(&active_tasks[index], &active_tasks[index + 1], (active_count - index - 1) * sizeof(Task));
        memmove(&active_pids[index], &active_pids[index + 1], (active_count - index - 1) * sizeof(pid_t));
    }
    active_count--;
}

void execute_task(Task task) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        char *args[] = { "/bin/sh", "-c", task.command, NULL };
        execv(args[0], args);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        active_tasks[active_count] = task;
        active_pids[active_count] = pid;
        active_count++;
    }
}

void monitor_tasks() {
    while (1) {
        for (int i = 0; i < active_count; ) {
            int status;
            pid_t result = waitpid(active_pids[i], &status, WNOHANG);
            if (result == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            } else if (result == 0) {
                i++;
            } else {
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
    monitor_tasks();  // Esta função deve ser executada em uma thread ou processo separado
    return 0;
}

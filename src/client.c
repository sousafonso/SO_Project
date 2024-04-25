#include "../includes/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PIPE_NAME "orchestrator_fifo"
#define FIFO_PATH "/tmp/orchestrator_fifo"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        // Enviar o comando para o servidor via FIFO
        int fifo_fd = open(PIPE_NAME, O_WRONLY);
        if (fifo_fd == -1) {
            perror("open");
            return 1;
        }

        if (write(fifo_fd, "status", strlen("status")) == -1) {
            perror("write");
            close(fifo_fd);
            return 1;
        }

        // Ler a resposta do servidor
        char buffer[4096];
        if (read(fifo_fd, buffer, sizeof(buffer)) == -1) {
            perror("read");
            close(fifo_fd);
            return 1;
        }

        printf("%s\n", buffer);

        close(fifo_fd);
        return 0;
    }

    int time_estimated;
    sscanf(argv[2], "%d", &time_estimated);

    // Juntar todos os argumentos em uma única string
    size_t total_length = 0;
    char command[300];
    snprintf(command, sizeof(command), "%d", time_estimated); // Adicionar o tempo estimado
    total_length += strlen(command) + 1; // +1 pelo espaço

    for (int i = 4; i < argc && total_length < sizeof(command); i++) {
        strncat(command, " ", sizeof(command) - total_length);
        strncat(command, argv[i], sizeof(command) - total_length - strlen(" "));
        total_length += strlen(argv[i]) + 1;
    }

    // Enviar o comando para o servidor via FIFO
    int fifo_fd = open(PIPE_NAME, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open");
        return 1;
    }

    if (write(fifo_fd, command, strlen(command)) == -1) {
        perror("write");
        close(fifo_fd);
        return 1;
    }

    // Ler o identificador da tarefa do servidor
    char task_id[100];
    if (read(fifo_fd, task_id, sizeof(task_id)) == -1) {
        perror("read");
        close(fifo_fd);
        return 1;
    }

    printf("Task ID: %s\n", task_id);

    close(fifo_fd);
    return 0;
}

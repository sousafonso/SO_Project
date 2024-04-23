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
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <time> <prog-a> [args]\n", argv[0]);
        return 1;
    }

    int time_estimated;
    sscanf(argv[1], "%d", &time_estimated);

    // Juntar todos os argumentos em uma única string
    size_t total_length = 0;
    char command[300];
    snprintf(command, sizeof(command), "%d", time_estimated); // Adicionar o tempo estimado
    total_length += strlen(command) + 1; // +1 pelo espaço

    strncat(command, " ", sizeof(command) - total_length);
    strncat(command, argv[2], sizeof(command) - total_length - strlen(" ")); // Adicionar o nome do programa
    total_length += strlen(argv[2]) + 1;

    for (int i = 3; i < argc && total_length < sizeof(command); i++) {
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

    close(fifo_fd);
    return 0;
}

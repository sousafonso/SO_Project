#include "../includes/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PIPE_NAME "orchestrator_fifo"

void execute_command(char *command) {
    char *args[1024];
    int i = 0;

    args[i] = strtok(command, "|");
    while (args[i] != NULL) {
        args[++i] = strtok(NULL, "|");
    }

    int num_pipes = i - 1;

    int pipe_fds[2 * num_pipes];
    for (i = 0; i < num_pipes; i++) {
        if (pipe(pipe_fds + i*2) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    int pid;
    int status;

    for (i = 0; i < num_pipes + 1; i++) {
        pid = fork();
        if (pid == 0) {
            if (i != 0) {
                if (dup2(pipe_fds[(i - 1) * 2], 0) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }

            if (i != num_pipes) {
                if (dup2(pipe_fds[i * 2 + 1], 1) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }

            for (int j = 0; j < 2 * num_pipes; j++) {
                close(pipe_fds[j]);
            }

            system(args[i]);

            exit(0);
        } else if (pid < 0) {
            perror("fork");
            exit(1);
        }
    }

    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipe_fds[i]);
    }

    for (int i = 0; i < num_pipes + 1; i++) {
        wait(&status);
    }
}

int start_client(int argc, char *argv[]) {
    if (argc < 3){
        fprintf(stderr, "Uso: client <tempo_estimado> <comando> [argumentos...]\n");
        return 1;
    }

    char command[1024];

    // Concatenar todos os argumentos em uma única string
    snprintf(command, sizeof(command), "%s", argv[2]);
    for (int i = 3; i < argc; i++) {
        strncat(command, " ", sizeof(command) - strlen(command) - 1);
        strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }

    // Assuma que o tamanho total dos argumentos passados à opção execute não excede os 300 bytes.
    if (strlen(command) > 300) {
        fprintf(stderr, "Erro: o input excede o limite de %d bytes.\n", 300);
        return 1;
    }

    char message[1024];
    snprintf(message, sizeof(message), "%s|%s", argv[1], command);

    int fifo_fd = open(PIPE_NAME, O_WRONLY);
    if (fifo_fd < 0) {
        perror("open");
        return 1;
    }

    if (write(fifo_fd, message, strlen(message) + 1) < 0) {
        perror("write");
        close(fifo_fd);
        return 1;
    }

    close(fifo_fd);
    return 0;
}

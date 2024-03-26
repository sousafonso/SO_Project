#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FIFO_NAME "orchestrator_fifo"

int client(int argc, char *argv[]) {
    if (argc < 4 || (strcmp(argv[1], "-u") != 0 && strcmp(argv[1], "-p") != 0)) {
        fprintf(stderr, "Uso: %s [-u | -p] tempo_em_ms \"programa1 [args1] | programa2 [args2] | ...\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char fifo[256];
    snprintf(fifo, sizeof(fifo), "%s", FIFO_NAME);

    int fd = open(fifo, O_WRONLY);
    if (fd == -1) {
        perror("Falha ao abrir o pipe nomeado");
        exit(EXIT_FAILURE);
    }

    char message[4096];
    snprintf(message, sizeof(message), "%s %s", argv[1], argv[2]);
    for (int i = 3; i < argc; ++i) {
        strncat(message, " ", sizeof(message) - strlen(message) - 1);
        strncat(message, argv[i], sizeof(message) - strlen(message) - 1);
    }

    if (write(fd, message, strlen(message)) == -1) {
        perror("Falha ao escrever no pipe nomeado");
        exit(EXIT_FAILURE);
    }

    printf("Tarefa submetida com sucesso.\n");

    close(fd);
    exit(EXIT_SUCCESS);
}


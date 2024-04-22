#include "../includes/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PIPE_NAME "orchestrator_fifo"

int start_client(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: client <tempo_estimado> <comando> [argumentos...]\n");
        return 1;
    }

    char command[1024];
    snprintf(command, sizeof(command), "%s|%s|%s", argv[0], argv[1], argv[2]);

    int fifo_fd = open(PIPE_NAME, O_WRONLY);
    if (fifo_fd < 0) {
        perror("open");
        return 1;
    }

    if (write(fifo_fd, command, strlen(command) + 1) < 0) {
        perror("write");
        close(fifo_fd);
        return 1;
    }

    close(fifo_fd);
    return 0;
}

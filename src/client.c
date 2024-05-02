#include "../includes/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FIFO_NAME "orchestrator_fifo"
#define FIFO_PATH "/tmp/" FIFO_NAME

int check_fifo_exists(const char* fifo_path) {
    struct stat st;
    if (stat(fifo_path, &st) == -1) {
        perror("stat");
        fprintf(stderr, "Certifique-se de que o orchestrator está rodando e criou o FIFO.\n");
        return 0; // Retorna 0 se o FIFO não existir
    }
    return 1; // Retorna 1 se o FIFO existir
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args]\n", argv[0]);
        return 1;
    }

    if (!check_fifo_exists(FIFO_PATH)) {
        return 1; // Encerra se o FIFO não existir
    }

    if (strcmp(argv[1], "status") == 0) {
        // Open the FIFO for writing
        int fifo_fd = open(FIFO_PATH, O_WRONLY);
        if (fifo_fd == -1) {
            perror("open");
            return 1;
        }

        // Send the status request to the server via FIFO
        char status_request[] = "status";
        if (write(fifo_fd, status_request, strlen(status_request)) == -1) {
            perror("write");
            close(fifo_fd);
            return 1;
        }

        // Close the FIFO
        close(fifo_fd);

        // Open the FIFO for reading
        fifo_fd = open(FIFO_PATH, O_RDONLY);
        if (fifo_fd == -1) {
            perror("open");
            return 1;
        }

        // Read the status response from the server
        char status_response[1024];
        if (read(fifo_fd, status_response, sizeof(status_response)) == -1) {
            perror("read");
            close(fifo_fd);
            return 1;
        }

        // Close the FIFO
        close(fifo_fd);

        // Print the status response
        printf("%s\n", status_response);

        return 0;
    }


    if (strcmp(argv[1], "execute") == 0) {
        if (argc < 4) {
        fprintf(stderr, "Usage: %s execute time -u 'prog-a [args]'\n", argv[0]);
        return 1;
        }

        // Open the FIFO for writing
        int fifo_fd = open(FIFO_PATH, O_WRONLY);
        if (fifo_fd == -1) {
            perror("open");
            return 1;
        }

        // Send the execute request to the server via FIFO
        char execute_request[1024];
        snprintf(execute_request, sizeof(execute_request), "%s %s %s \"%s\"", argv[1], argv[2], argv[3], argv[4]);
        if (write(fifo_fd, execute_request, strlen(execute_request)) == -1) {
            perror("write");
            close(fifo_fd);
            return 1;
        }

        // Close the FIFO
        close(fifo_fd);

        printf("Task submitted successfully.\n");

        return 0;
    }

    fprintf(stderr, "Invalid command '%s'. Supported commands: status, execute.\n", argv[1]);
    return 1;
}


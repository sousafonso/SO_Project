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
    // Open the FIFO for writing
    int fifo_fd = open(PIPE_NAME, O_WRONLY);
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
    fifo_fd = open(PIPE_NAME, O_RDONLY);
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

        int time_estimated;
        if (sscanf(argv[2], "%d", &time_estimated) != 1 || time_estimated <= 0) {
            fprintf(stderr, "Invalid estimated time. Please provide a positive integer.\n");
            return 1;
        }

        char command[300] = "";
        strcat(command, argv[3]); // prog-a

        // Concatenar todos os argumentos restantes em uma única string
        for (int i = 4; i < argc; i++) {
            strcat(command, " ");
            strcat(command, argv[i]);
        }

        printf("Estimated time: %d ms\n", time_estimated);
        printf("Command to execute: %s\n", command);

        // Open the FIFO for writing
        int fifo_fd = open(PIPE_NAME, O_WRONLY);
        if (fifo_fd == -1) {
            perror("open");
            return 1;
        }

        // Send the command to the server via FIFO
        if (write(fifo_fd, command, strlen(command)) == -1) {
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


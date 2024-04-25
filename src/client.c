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

        // Send the status command to the server via FIFO
        if (write(fifo_fd, "status", strlen("status")) == -1) {
            perror("write");
            close(fifo_fd);
            return 1;
        }

        // Read the response from the server
        char buffer[4096];
        if (read(fifo_fd, buffer, sizeof(buffer)) == -1) {
            perror("read");
            close(fifo_fd);
            return 1;
        }

        // Print the status output
        printf("%s\n", buffer);

        // Close the FIFO
        close(fifo_fd);
        return 0;
    }

    if (strcmp(argv[1], "execute") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s execute tempo_em_ms [-u \"program args\"] [-p \"program1 args1 | program2 args2 | ...\"]\n", argv[0]);
            return 1;
        }

        int time_estimated;
        if (sscanf(argv[2], "%d", &time_estimated) != 1 || time_estimated <= 0) {
            fprintf(stderr, "Invalid estimated time. Please provide a positive integer.\n");
            return 1;
        }

        char *command = NULL;
        int i;
        for (i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "-p") == 0) {
                if (i + 1 < argc) {
                    command = argv[i + 1];
                    break;
                } else {
                    fprintf(stderr, "Missing program and arguments after '%s'.\n", argv[i]);
                    return 1;
                }
            }
        }

        if (command == NULL) {
            fprintf(stderr, "Invalid command. Use '-u \"program args\"' or '-p \"program1 args1 | program2 args2 | ...\"'.\n");
            return 1;
        }

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


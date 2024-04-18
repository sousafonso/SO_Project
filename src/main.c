#include "../includes/client.h"
#include "../includes/orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s [client | server]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "client") == 0) {
        return client(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "server") == 0) {
        return orchestrator(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Opção inválida: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
}


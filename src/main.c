#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../includes/orchestrator.h"
#include "../includes/client.h"

// Protótipos das funções dos outros arquivos
int start_server();
int start_client(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s server|client [args...]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        return start_server();
    } else if (strcmp(argv[1], "client") == 0) {
        return start_client(argc - 2, argv + 2);
    } else {
        fprintf(stderr, "Argumento inválido: %s\n", argv[1]);
        fprintf(stderr, "Uso: %s server|client [args...]\n", argv[0]);
        return 1;
    }
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../includes/orchestrator.h"
#include "../includes/client.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <server|client> [args...]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        return start_server();
    } else if (strcmp(argv[1], "client") == 0) {
        return start_client(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Comando inválido.\n");
        return 1;
    }

    main_loop();

    return 0;
}

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>

void submit_task (int client_socket) {
    char buffer[1024];
    int n;

    while (1) {
        n = recv (client_socket, buffer, sizeof (buffer), 0);
        if (n <= 0) {
            break;
        }
        send (client_socket, buffer, n, 0);
    }
}

void get_task (int server_socket) {
    char buffer[1024];
    int n;

    while (1) {
        n = read (0, buffer, sizeof (buffer));
        if (n <= 0) {
            break;
        }
        send (server_socket, buffer, n, 0);
        n = recv (server_socket, buffer, sizeof (buffer), 0);
        write (1, buffer, n);
    }
}

void get_results (int server_socket) {
    char buffer[1024];
    int n;

    while (1) {
        n = recv (server_socket, buffer, sizeof (buffer), 0);
        if (n <= 0) {
            break;
        }
        write (1, buffer, n);
    }
}

void view_log (int server_socket) {
    char buffer[1024];
    int n;

    while (1) {
        n = recv (server_socket, buffer, sizeof (buffer), 0);
        if (n <= 0) {
            break;
        }
        write (1, buffer, n);
    }
}

void cancel_task (int server_socket) {
    char buffer[1024];
    int n;

    while (1) {
        n = read (0, buffer, sizeof (buffer));
        if (n <= 0) {
            break;
        }
        send (server_socket, buffer, n, 0);
    }
}

void list_tasks (int server_socket) {
    char buffer[1024];
    int n;

    while (1) {
        n = recv (server_socket, buffer, sizeof (buffer), 0);
        if (n <= 0) {
            break;
        }
        write (1, buffer, n);
    }
}

int main (int Argc, char* Argv[]) {
    int server_socket, client_socket; // server_socket is the socket used to listen for incoming connections, client_socket is the socket used to communicate with the client
    struct sockaddr_in server_addr, client_addr; // define os endereços do servidor e do cliente
    int client_addr_len; // tamanho do endereço do cliente
    int pid; // id do processo

    server_socket = socket (AF_INET, SOCK_STREAM, 0); // cria um socket do tipo TCP (SOCK_STREAM) e do protocolo IP (AF_INET) e retorna um descritor de arquivo
    if (server_socket < 0) {
        perror ("socket");
        exit (1);
    }

    server_addr.sin_family = AF_INET; // define o protocolo de endereçamento
    server_addr.sin_addr.s_addr = htonl (INADDR_ANY); // define o endereço do servidor
    server_addr.sin_port = htons (1234); // define a porta do servidor

    if (bind (server_socket, (struct sockaddr*)&server_addr, sizeof (server_addr)) < 0) { // associa o socket com o endereço do servidor
        perror ("bind");
        exit (1);
    }

    if (listen (server_socket, 5) < 0) { // coloca o socket em modo de escuta
        perror ("listen");
        exit (1);
    }

    while (1) {
        client_addr_len = sizeof (client_addr);
        client_socket = accept (server_socket, (struct sockaddr*)&client_addr, &client_addr_len); // aceita uma conexão de um cliente e retorna um novo socket para comunicação
        if (client_socket < 0) {
            perror ("accept");
            exit (1);
        }

        pid = fork (); // cria um novo processo para lidar com a conexão
        if (pid < 0) {
            perror ("fork");
            exit (1);
        }

        if (pid == 0) {
            close (server_socket);
            submit_task (client_socket);
            close (client_socket);
            exit (0);
        } else {
            close (client_socket);
            waitpid (pid, NULL, WNOHANG);
        }
    }
}
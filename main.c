#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define PORT 1234
#define BUFFER_SIZE 1024
#define MAX_TASKS 256 // Limite arbitrário para o número de tarefas

int task_pids[MAX_TASKS]; // Array para armazenar os PIDs das tarefas
int task_count = 0; // Contador para manter o número de tarefas submetidas

/**
 * @brief FUNÇÕES UTILIZADAS
 * 
 * @kill - envia um sinal para um processo
 * @fork - cria um novo processo para lidar com a conexão
 * @execl - executa um arquivo 
 * @open - abre um arquivo
 * @dup2 - duplica um descritor de arquivo para outro
 * @close - fecha um descritor de arquivo 
 * @socket - cria um socket para comunicação
 * @bind - associa um socket a um endereço para comunicação
 * @listen - coloca um socket em modo de escuta para aceitar conexões
 * @accept - aceita uma conexão de um cliente 
 * @recv - recebe dados de um socket
 * @send - envia dados para um socket
 * 
 * socket aqui significa um ponto final para comunicação entre dois processos
*/

/**
 * @brief função que submete uma tarefa
 * @arg client_socket: socket do cliente
 * @arg command: comando a ser executado
*/
void handle_submit_task(int client_socket, char* command) {
    if (task_count >= MAX_TASKS) {
        char* msg = "Limite máximo de tarefas atingido.\n";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }

    int pid = fork();
    if (pid == 0) { // Processo filho
        // Redirecionar stdout e stderr para arquivos
        char output_file[64];
        snprintf(output_file, sizeof(output_file), "task_%d_output.txt", task_count);
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        // Executar o comando
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        exit(EXIT_FAILURE); // execl só retorna se falhar
    } else if (pid > 0) { // Processo pai
        task_pids[task_count] = pid; // Armazenar o PID da tarefa
        char msg[64];
        snprintf(msg, sizeof(msg), "Tarefa %d submetida com sucesso.\n", task_count);
        send(client_socket, msg, strlen(msg), 0);
        task_count++;
    } else {
        perror("fork");
    }
}

/**
 * @brief função que cancela uma tarefa
 * @arg client_socket: socket do cliente
 * @arg taskIdStr: id da tarefa
*/
void handle_cancel_task(int client_socket, char* taskIdStr) {
    int taskId = atoi(taskIdStr);
    if (taskId < 0 || taskId >= task_count) {
        char* msg = "ID de tarefa inválido.\n";
        send(client_socket, msg, strlen(msg), 0);
        return;
    }

    int pid = task_pids[taskId];
    if (pid > 0) {
        kill(pid, SIGTERM); 
        char msg[64];
        snprintf(msg, sizeof(msg), "Tarefa %d cancelada.\n", taskId);
        send(client_socket, msg, strlen(msg), 0);
    } else {
        char* msg = "Tarefa já terminada ou ID inválido.\n";
        send(client_socket, msg, strlen(msg), 0);
    }
}

/**
 * @brief função que processa o pedido do cliente
 * @arg client_socket: socket do cliente
*/
void process_client_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t n;

    memset(buffer, 0, BUFFER_SIZE);
    n = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (n <= 0) {
        return;
    }

    // Determinar o tipo de comando (irão aparecer mais evidentemente, é só construir)
    if (strncmp(buffer, "SUBMIT", 6) == 0) {
        handle_submit_task(client_socket, buffer + 7); // +7 para saltar "SUBMIT " e ir direto ao comando
    } else if (strncmp(buffer, "CANCEL", 6) == 0) {
        handle_cancel_task(client_socket, buffer + 7); // +7 para saltar "CANCEL " e ir direto ao ID da tarefa
    }
}

/**
 * @brief VARIAVEIS UTILIZADAS
 * 
 * @task_pids - array para armazenar os PIDs das tarefas
 * @task_count - contador para manter o número de tarefas submetidas
 * @MAX_TASKS - limite arbitrário para o número de tarefas
 * @PORT - porta do servidor
 * @BUFFER_SIZE - tamanho do buffer
 * @server_socket - socket do servidor
 * @client_socket - socket do cliente
 * @server_addr - endereço do servidor
 * @client_addr - endereço do cliente
 * @client_addr_len - tamanho do endereço do cliente
 * @pid - id do processo
*/
int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Erro ao criar o socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao fazer bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Erro ao escutar");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado na porta %d\n", PORT);

    while (1) {
        client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Erro ao aceitar conexão");
            continue;
        }

        int pid = fork();
        if (pid < 0) {
            perror("Erro no fork");
            close(client_socket);
        } else if (pid == 0) { // Processo filho
            close(server_socket);
            process_client_request(client_socket);
            close(client_socket);
            exit(0);
        } else { // Processo pai
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}

// void submit_task (int client_socket) {
//     char buffer[1024];
//     int n;

//     while (1) {
//         n = recv (client_socket, buffer, sizeof (buffer), 0);
//         if (n <= 0) {
//             break;
//         }
//         send (client_socket, buffer, n, 0);
//     }
// }

// void get_task (int server_socket) {
//     char buffer[1024];
//     int n;

//     while (1) {
//         n = read (0, buffer, sizeof (buffer));
//         if (n <= 0) {
//             break;
//         }
//         send (server_socket, buffer, n, 0);
//         n = recv (server_socket, buffer, sizeof (buffer), 0);
//         write (1, buffer, n);
//     }
// }

// void get_results (int server_socket) {
//     char buffer[1024];
//     int n;

//     while (1) {
//         n = recv (server_socket, buffer, sizeof (buffer), 0);
//         if (n <= 0) {
//             break;
//         }
//         write (1, buffer, n);
//     }
// }

// void view_log (int server_socket) {
//     char buffer[1024];
//     int n;

//     while (1) {
//         n = recv (server_socket, buffer, sizeof (buffer), 0);
//         if (n <= 0) {
//             break;
//         }
//         write (1, buffer, n);
//     }
// }

// void cancel_task (int server_socket) {
//     char buffer[1024];
//     int n;

//     while (1) {
//         n = read (0, buffer, sizeof (buffer));
//         if (n <= 0) {
//             break;
//         }
//         send (server_socket, buffer, n, 0);
//     }
// }

// void list_tasks (int server_socket) {
//     char buffer[1024];
//     int n;

//     while (1) {
//         n = recv (server_socket, buffer, sizeof (buffer), 0);
//         if (n <= 0) {
//             break;
//         }
//         write (1, buffer, n);
//     }
// }

// int main (int Argc, char* Argv[]) {
//     int server_socket, client_socket; // server_socket is the socket used to listen for incoming connections, client_socket is the socket used to communicate with the client
//     struct sockaddr_in server_addr, client_addr; // define os endereços do servidor e do cliente
//     int client_addr_len; // tamanho do endereço do cliente
//     int pid; // id do processo

//     server_socket = socket (AF_INET, SOCK_STREAM, 0); // cria um socket do tipo TCP (SOCK_STREAM) e do protocolo IP (AF_INET) e retorna um descritor de arquivo
//     if (server_socket < 0) {
//         perror ("socket");
//         exit (1);
//     }

//     server_addr.sin_family = AF_INET; // define o protocolo de endereçamento
//     server_addr.sin_addr.s_addr = htonl (INADDR_ANY); // define o endereço do servidor
//     server_addr.sin_port = htons (1234); // define a porta do servidor

//     if (bind (server_socket, (struct sockaddr*)&server_addr, sizeof (server_addr)) < 0) { // associa o socket com o endereço do servidor
//         perror ("bind");
//         exit (1);
//     }

//     if (listen (server_socket, 5) < 0) { // coloca o socket em modo de escuta
//         perror ("listen");
//         exit (1);
//     }

//     while (1) {
//         client_addr_len = sizeof (client_addr);
//         client_socket = accept (server_socket, (struct sockaddr*)&client_addr, &client_addr_len); // aceita uma conexão de um cliente e retorna um novo socket para comunicação
//         if (client_socket < 0) {
//             perror ("accept");
//             exit (1);
//         }

//         pid = fork (); // cria um novo processo para lidar com a conexão
//         if (pid < 0) {
//             perror ("fork");
//             exit (1);
//         }

//         if (pid == 0) {
//             close (server_socket);
//             submit_task (client_socket);
//             close (client_socket);
//             exit (0);
//         } else {
//             close (client_socket);
//             waitpid (pid, NULL, WNOHANG);
//         }
//     }
// }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../includes/orchestrator.h"
#include "../includes/client.h"

int main(int argc, char *argv[]) {
    // Executar o orchestrator no terminal atual
    system("gnome-terminal -- ./orchestrator output/ 3 FIFO");
    
    sleep(1);

    // Abrir um novo terminal e executar o client com o comando status
    system("gnome-terminal -- ./client execute 100 -u \"ls -l\"");

    // Aguardar 1 segundo para garantir que o client tenha iniciado
    sleep(1);
    
    // Abrir um novo terminal e executar o client com o comando status
    system("./client execute 20000 -p \"grep 'pattern' | ls -l\"");

    // Aguardar 1 segundo para garantir que o client tenha iniciado
    sleep(1);
    
    // Abrir um novo terminal e executar o client com o comando status
    system("gnome-terminal -- bash -c './client status; read -p \"Press Enter to continue\"'");

     // Aguardar 2 segundos para garantir que o client tenha iniciado
    sleep(1);

    // Usar expect para enviar uma pressão de tecla "Enter" para o client
//    system("expect -c 'spawn -noecho ./client status; send \"\\r\"; interact'");

    return 0;
}

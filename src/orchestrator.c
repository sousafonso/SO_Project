#include "../includes/orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FIFO_NAME "orchestrator_fifo"
#define FIFO_PATH "/tmp/" FIFO_NAME
#define MAX_COMMAND_SIZE 4096
int MAX_TASKS = 0; 

//CompletedTask completed_tasks[MAX_TASKS];
int completed_count = 0;

//Task waiting_queue[MAX_TASKS];
int waiting_count = 0;

//ActiveTask active_tasks[MAX_TASKS];
int active_count = 0;
//pid_t active_pids[MAX_TASKS];

struct TaskStartTime *task_start_times;
struct CompletedTask *completed_tasks;
Task *waiting_queue;
ActiveTask *active_tasks;
pid_t *active_pids;


pthread_mutex_t lock;

void handle_status_command(int fifo_fd) {
    char status_message[4096] = "Tarefas em espera e ativas:\n";

    // Adicionar o status das tarefas em espera ao status_message
    strcat(status_message, "Tarefas em espera:\n");
    for (int i = 0; i < waiting_count; i++) {
        char task_info[256];
        snprintf(task_info, sizeof(task_info), "Task ID: %s, Command: %s\n", waiting_queue[i].id, waiting_queue[i].command);
        strcat(status_message, task_info);
    }

    // Adicionar o status das tarefas ativas ao status_message
    strcat(status_message, "\nTarefas ativas:\n");
    for (int i = 0; i < active_count; i++) {
        char task_info[256];
        snprintf(task_info, sizeof(task_info), "Task ID: %s, Command: %s, PID: %d\n", active_tasks[i].task.id, active_tasks[i].task.command, active_pids[i]);
        strcat(status_message, task_info);
    }
    // Adicionar o status das tarefas concluídas
    strcat(status_message, "\nTarefas concluídas:\n");
    int completed_file = open("output/completed_tasks.txt", O_RDONLY);
    if (completed_file != -1) {
        char line[256];
	ssize_t n;
        while ((n = read(completed_file, line, sizeof(line) - 1)) > 0) {
            line[n] = '\0';
            strcat(status_message, line);
        }
        close(completed_file);
    }
    // Envia a mensagem de status de volta para o cliente
    if (write(fifo_fd, status_message, strlen(status_message) + 1) == -1) {
        perror("write");
    }
}

void add_active_task(ActiveTask active_task) {
    pthread_mutex_lock(&lock);
    active_tasks[active_count] = active_task;
    active_pids[active_count] = active_task.pid;
    active_count++;
    pthread_mutex_unlock(&lock);
}

void setup_communication(const char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }
}

void parse_client_request(const char *buffer, Task *task) {
    if (buffer == NULL || task == NULL) {
        fprintf(stderr, "Erro: buffer ou task não podem ser nulos.\n");
        return;
    }

    char execute_keyword[8];
    char time_keyword[3];
    char command_buffer[300]; // Defina um tamanho adequado para o seu comando

    // Primeiro tenta ler a palavra "execute" e o tempo estimado
    int num_args_scanned = sscanf(buffer, "%7s %d %2s", execute_keyword, &task->estimated_time, time_keyword);
    if (num_args_scanned != 3 || strcmp(execute_keyword, "execute") != 0 || (strcmp(time_keyword, "-u") != 0 && strcmp(time_keyword, "-p") != 0)) {
        fprintf(stderr, "Erro: comando inválido ou tempo estimado ausente.\n");
        return;
    }

    // Após ler o tempo estimado, move o buffer para frente, além do tempo
    buffer = strchr(buffer, ' '); // Encontra o espaço após "execute"
    buffer = strchr(buffer + 1, ' '); // Encontra o espaço após o tempo estimado
    buffer = strchr(buffer + 1, ' '); // Encontra o espaço após "-u" ou "-p"
    if (!buffer) {
        fprintf(stderr, "Erro: formato de comando inválido.\n");
        return;
    }
    buffer++; // Avança para o primeiro caractere do comando

    // Agora, tenta ler o comando entre aspas
    if (sscanf(buffer, "\"%[^\"]\"", command_buffer) != 1) {
        fprintf(stderr, "Erro: comando ausente ou não entre aspas.\n");
        return;
    }

    // Copia o comando para a estrutura Task
    strncpy(task->command, command_buffer, sizeof(task->command) - 1);
    task->command[sizeof(task->command) - 1] = '\0'; // Garante o terminador nulo
}

void save_state() {
    int fd = open("state.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    FILE *file = fdopen(fd, "w");
    if (file == NULL) {
        perror("fdopen");
        close(fd);
        exit(EXIT_FAILURE);
    }

    fprintf(file, "Waiting tasks:\n");
    for (int i = 0; i < waiting_count; i++) {
        fprintf(file, "Task ID: %s, Command: %s\n", waiting_queue[i].id, waiting_queue[i].command);
    }

    fprintf(file, "\nActive tasks:\n");
    for (int i = 0; i < active_count; i++) {
        fprintf(file, "Task ID: %s, Command: %s, PID: %d\n", active_tasks[i].task.id, active_tasks[i].task.command, active_tasks[i].pid);
    }

    fclose(file);
}
/*
void enqueue_task(Task task) {
    if (waiting_count >= MAX_TASKS) {
        fprintf(stderr, "Fila de espera cheia. Tarefa %s não enfileirada.\n", task.id);
        return;
    }

    waiting_queue[waiting_count++] = task;
    save_state();

    // Iniciar a tarefa se houver espaço e a fila de espera não estiver vazia
    if (active_count < MAX_TASKS && waiting_count > 0) {
        Task next_task = dequeue_task();
        execute_task(next_task);
    }
}
*/
Task dequeue_task() {
    if (waiting_count == 0) {
        fprintf(stderr, "Fila de espera vazia.\n");
        exit(EXIT_FAILURE);
    }

    Task task = waiting_queue[0];
    for (int i = 1; i < waiting_count; i++) {
        waiting_queue[i - 1] = waiting_queue[i];
    }
    waiting_count--;
    save_state();
    return task;
}

void enqueue_task(Task task) {

    static int task_counter = 0;

    // Calcular o tamanho necessário para o ID da tarefa
    int id_length = snprintf(NULL, 0, "%d", task_counter) + 1;

    // Alocar memória para o ID da tarefa e gerar o ID
    char *new_id = (char *)malloc(id_length);
    if (new_id == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memória para o ID da tarefa.\n");
        exit(EXIT_FAILURE);
    }
    snprintf(new_id, id_length, "%d", task_counter);

    // Copiar o ID gerado para a tarefa
    strncpy(task.id, new_id, sizeof(task.id) - 1);
    task.id[sizeof(task.id) - 1] = '\0'; // Garantir terminador nulo

    // Incrementar o contador para a próxima tarefa
    task_counter++;


    // Verificar se a tarefa recém-adicionada tem um tempo de execução estimado menor
    if (active_count > 0 && task.estimated_time < active_tasks[0].task.estimated_time) {
        fprintf(stderr, "Tarefa de maior prioridade adicionada. Interrompendo tarefa ativa...\n");

        // Interromper a tarefa ativa atual
        ActiveTask active_task = active_tasks[0];
        remove_active_task(0); // Remove a tarefa ativa sem matar o processo

        // Coloca a tarefa ativa na fila de espera
        waiting_queue[waiting_count++] = active_task.task;
    }

    // Se não houver tarefas ativas, inicie esta tarefa imediatamente
    if (active_count < MAX_TASKS) {
        execute_task(task);
    } else {

        fprintf(stderr, "Fila de tarefas ativas cheia. Tarefa %s enfileirada.\n", task.id);
        waiting_queue[waiting_count++] = task;
        return;
        save_state();
    }
}

void remove_active_task(int index) {
    if (index < 0 || index >= active_count) {
        fprintf(stderr, "Índice de tarefa ativa inválido.\n");
        return;
    }

    // Obter o tempo atual
    struct timeval now;
    gettimeofday(&now, NULL);

    // Calcular o tempo de execução da tarefa
    long execution_time = (now.tv_sec - active_tasks[index].start_time.tv_sec) * 1000 +
                          (now.tv_usec - active_tasks[index].start_time.tv_usec) / 1000;

    // Registar a tarefa num arquivo
    int fd = open("output/completed_tasks.txt", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        return;
    }
    FILE *file = fdopen(fd, "a");
    if (file == NULL) {
        perror("fdopen");
        close(fd);
        return;
    }
    fprintf(file, "Task ID: %s, Command: %s, Execution time: %ld ms\n", active_tasks[index].task.id, active_tasks[index].task.command, execution_time);
    fclose(file);

    if (waiting_count > 0) {
        Task next_task = dequeue_task();
        execute_task(next_task);
    }    

    if (index < active_count - 1) {
        memmove(&active_tasks[index], &active_tasks[index + 1], (active_count - index - 1) * sizeof(ActiveTask));
    }
    active_count--;
    save_state();
    

}

void monitor_active_tasks() {
    // Enquanto houver tarefas ativas para monitorar
    while (active_count > 0) {
        // Aguardar 1 segundo antes de verificar novamente
        sleep(1);

        // Obter o tempo atual
        struct timeval now;
        gettimeofday(&now, NULL);

        // Iterar sobre as tarefas ativas
        for (int i = 0; i < active_count; i++) {
            ActiveTask *active_task = &active_tasks[i];
            
            // Calcular o tempo de execução da tarefa
            long execution_time = (now.tv_sec - active_task->start_time.tv_sec) * 1000 +
                                  (now.tv_usec - active_task->start_time.tv_usec) / 1000;

            // Se a tarefa excedeu o tempo estimado, removê-la da lista de tarefas ativas
            if (execution_time >= active_task->task.estimated_time) {
                remove_active_task(i);
            }
        }
    }
}


void execute_task(Task task) {
    // Criar processo filho para executar a tarefa
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Processo filho: executar o comando da tarefa usando system
        printf("Executando tarefa: %s\n", task.command);
        int result = system(task.command);
        if (result == -1) {
            perror("system");
            exit(EXIT_FAILURE); // Em caso de falha na execução do comando
        }
        exit(EXIT_SUCCESS); // Saída bem-sucedida do processo filho
    } else {
        // Processo pai: adicionar tarefa à lista de tarefas ativas
        ActiveTask active_task = {.task = task, .pid = pid, .start_time = {0}};
        gettimeofday(&active_task.start_time, NULL);
        add_active_task(active_task);
    }
}


void monitor_tasks() {
    // Enquanto houver tarefas ativas para monitorar
    while (active_count > 0) {
        // Iterar sobre as tarefas ativas
        for (int i = 0; i < active_count; i++) {
            int status;
            pid_t pid = waitpid(active_pids[i], &status, WNOHANG);
            if (pid == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                // Processo filho terminou
                remove_active_task(i);
            }
        }

        // Enquanto houver tarefas na fila de espera e espaço para tarefas ativas
        while (waiting_count > 0 && active_count < MAX_TASKS) {
            Task next_task = dequeue_task();
            execute_task(next_task);
        }

        // Aguardar 1 segundo antes de verificar novamente
        sleep(1);
    }
}



int start_server() {
    setup_communication(FIFO_PATH);
    printf("Servidor iniciado.\n");

    // Iniciar uma thread para monitorar as tarefas ativas
    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, (void *(*)(void *))monitor_active_tasks, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    
    int fifo_fd = open(FIFO_PATH, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    ssize_t num_read;

    while (1) {
        num_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        if (num_read == -1) {
            perror("read");
            continue;
        } else if (num_read == 0) {
            continue; 
        }

        buffer[num_read] = '\0'; // Terminar o buffer com nulo

        if (strcmp(buffer, "status") == 0) {
            int write_fd = open(FIFO_PATH, O_WRONLY);
            if (write_fd == -1) {
                perror("open");
                continue;
            }
            handle_status_command(write_fd);
            close(write_fd);
        } else {
            Task task;
            parse_client_request(buffer, &task);
            enqueue_task(task);
        }
    }

    close(fifo_fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s output_folder parallel_tasks sched_policy\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *output_folder = argv[1];
    int parallel_tasks = atoi(argv[2]);
    char *sched_policy = argv[3];

    // Verifique se output_folder é um diretório válido
    struct stat s;
    if (stat(output_folder, &s) != 0 || !S_ISDIR(s.st_mode)) {
        fprintf(stderr, "Erro: %s não é um diretório válido.\n", output_folder);
        exit(EXIT_FAILURE);
    }

    // Verifique se parallel_tasks é um número válido
    if (parallel_tasks <= 0) {
        fprintf(stderr, "Erro: parallel_tasks deve ser um número maior que 0.\n");
        exit(EXIT_FAILURE);
    }

    // Verifique se sched_policy é uma política de escalonamento válida
    if (strcmp(sched_policy, "FIFO") != 0 && strcmp(sched_policy, "RR") != 0) {
        fprintf(stderr, "Erro: sched_policy deve ser 'FIFO' ou 'RR'.\n");
        exit(EXIT_FAILURE);
    }

    MAX_TASKS = parallel_tasks;


    task_start_times = malloc(MAX_TASKS * sizeof(struct TaskStartTime));
    completed_tasks = malloc(MAX_TASKS * sizeof(CompletedTask));
    waiting_queue = malloc(MAX_TASKS * sizeof(Task));
    active_tasks = malloc(MAX_TASKS * sizeof(ActiveTask));
    active_pids = malloc(MAX_TASKS * sizeof(pid_t));
    

    // Verifique se a alocação de memória foi bem-sucedida
    if (task_start_times == NULL || completed_tasks == NULL || waiting_queue == NULL || active_tasks == NULL || active_pids == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memória para os arrays dinâmicos.\n");
        exit(EXIT_FAILURE);
    }

    // Inicie o servidor com os argumentos fornecidos
    start_server(output_folder, parallel_tasks, sched_policy);

    // Libere a memória alocada dinamicamente no final do programa
    free(task_start_times);
    free(completed_tasks);
    free(waiting_queue);
    free(active_tasks);
    free(active_pids);

    return 0;
}

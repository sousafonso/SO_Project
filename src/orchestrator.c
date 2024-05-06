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
#include <stdbool.h>

// Defina uma variável global para indicar se o servidor deve continuar em execução
bool server_running = true;

#define FIFO_NAME "orchestrator_fifo"
#define FIFO_PATH "/tmp/" FIFO_NAME
#define MAX_COMMAND_SIZE 4096
int MAX_TASKS = 0; 

int completed_count = 0;
int waiting_count = 0;
int active_count = 0;

struct TaskStartTime *task_start_times;
struct CompletedTask *completed_tasks;
Task *waiting_queue;
ActiveTask *active_tasks;
pid_t *active_pids;

void handle_status_command(int fifo_fd) {
    char status_message[4096] = "Tarefas em espera, ativas e concluídas:\n";

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
    if (active_count >= MAX_TASKS) {
        printf("Número máximo de tarefas ativas atingido.\n");
        return;
    }

    active_tasks[active_count] = active_task;
    active_pids[active_count] = active_task.pid;
    active_count++;
    save_state();
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
        printf("Erro: buffer ou task não podem ser nulos.\n");
        return;
    }

    char execute_keyword[8];
    char time_keyword[3];
    char command_buffer[300];

    // Primeiro tenta ler a palavra "execute" e o tempo estimado
    int num_args_scanned = sscanf(buffer, "%7s %d %2s", execute_keyword, &task->estimated_time, time_keyword);
    if (num_args_scanned != 3 || strcmp(execute_keyword, "execute") != 0 || (strcmp(time_keyword, "-u") != 0 && strcmp(time_keyword, "-p") != 0)) {
        printf("Erro: comando inválido ou tempo estimado ausente.\n");
        return;
    }

    // Após ler o tempo estimado, move o buffer para frente, além do tempo
    buffer = strchr(buffer, ' ');
    buffer = strchr(buffer + 1, ' ');
    buffer = strchr(buffer + 1, ' ');
    if (!buffer) {
        printf("Erro: formato de comando inválido.\n");
        return;
    }
    buffer++;

    // Agora, tenta ler o comando entre aspas
    if (sscanf(buffer, "\"%[^\"]\"", command_buffer) != 1) {
        printf("Erro: comando ausente ou não entre aspas.\n");
        return;
    }

    // Copia o comando para a estrutura Task
    strncpy(task->command, command_buffer, sizeof(task->command) - 1);
    task->command[sizeof(task->command) - 1] = '\0'; 
}

void save_state() {
    int fd = open("state.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[4096];

    // Escrever tarefas em espera
    int offset = sprintf(buffer, "Waiting tasks:\n");
    for (int i = 0; i < waiting_count; i++) {
        offset += sprintf(buffer + offset, "Task ID: %s, Command: %s\n", waiting_queue[i].id, waiting_queue[i].command);
    }

    // Escrever tarefas ativas
    offset += sprintf(buffer + offset, "\nActive tasks:\n");
    for (int i = 0; i < active_count; i++) {
        offset += sprintf(buffer + offset, "Task ID: %s, Command: %s, PID: %d\n", active_tasks[i].task.id, active_tasks[i].task.command, active_tasks[i].pid);
    }

    // Escrever o conteúdo do buffer no arquivo
    if (write(fd, buffer, offset) == -1) {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

Task dequeue_task() {
    if (waiting_count == 0) {
        printf("Fila de espera vazia.\n");
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

    int id_length = snprintf(NULL, 0, "%d", task_counter) + 1;

    char *new_id = (char *)malloc(id_length);
    if (new_id == NULL) {
        printf("Erro: falha ao alocar memória para o ID da tarefa.\n");
        exit(EXIT_FAILURE);
    }
    snprintf(new_id, id_length, "%d", task_counter);

    strncpy(task.id, new_id, sizeof(task.id) - 1);
    task.id[sizeof(task.id) - 1] = '\0'; 

    task_counter++;

    int insert_index = 0;
    while (insert_index < waiting_count && task.estimated_time > waiting_queue[insert_index].estimated_time) {
        insert_index++;
    }

    if (active_count < MAX_TASKS) {
        execute_task(task);
    } else {
        if (waiting_count < MAX_TASKS) {
            for (int i = waiting_count; i > insert_index; i--) {
                waiting_queue[i] = waiting_queue[i - 1];
            }
            waiting_queue[insert_index] = task;
            waiting_count++;
            printf("Tarefa %s enfileirada para espera.\n", task.id);
            save_state();
        }
    }
}

void remove_active_task(int index) {
    if (index < 0 || index >= active_count) {
        printf("Índice de tarefa ativa inválido.\n");
        return;
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    long execution_time = (now.tv_sec - active_tasks[index].start_time.tv_sec) * 1000 +
                          (now.tv_usec - active_tasks[index].start_time.tv_usec) / 1000;

    if (execution_time >= active_tasks[index].task.estimated_time) {
        char buffer[4096];
        int offset = sprintf(buffer, "Task ID: %s, Command: %s, Execution time: %ld ms\n", 
                             active_tasks[index].task.id, active_tasks[index].task.command, execution_time);

        int fd = open("output/completed_tasks.txt", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("open");
            return;
        }

        if (write(fd, buffer, offset) == -1) {
            perror("write");
            close(fd);
            return;
        }

        close(fd);

        for (int i = index; i < active_count - 1; i++) {
            active_tasks[i] = active_tasks[i + 1];
            active_pids[i] = active_pids[i + 1];
        }
        active_count--;

        if (waiting_count > 0 && active_count < MAX_TASKS) {
            Task next_task = dequeue_task();
            execute_task(next_task);
        }

        save_state();
    }
}

void execute_task(Task task) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        close(pipe_fd[0]); // Fecha a extremidade de leitura do pipe no processo filho

        // Redireciona a saída padrão (stdout) para o pipe
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        close(pipe_fd[1]); // Fecha a extremidade de escrita do pipe no processo filho

        // Executa o comando da tarefa
        execlp("sh", "sh", "-c", task.command, NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        close(pipe_fd[1]); // Fecha a extremidade de escrita do pipe no processo pai

        ActiveTask active_task = {.task = task, .pid = pid, .start_time = {0}};
        gettimeofday(&active_task.start_time, NULL);
        add_active_task(active_task);
        printf("Nova tarefa iniciada: Task ID: %s, Command: %s, PID: %d\n", task.id, task.command, pid);

        close(pipe_fd[0]); // Fecha a extremidade de leitura do pipe no processo pai
    }
}

void monitor_active_tasks() {
    while (server_running) {
        usleep(100000); // Aguarda 100ms para verificar novamente

        if (active_count > 0) {
            for (int i = 0; i < active_count; i++) {
                ActiveTask *active_task = &active_tasks[i];

                struct timeval now;
                gettimeofday(&now, NULL);
                long execution_time = (now.tv_sec - active_task->start_time.tv_sec) * 1000 +
                                      (now.tv_usec - active_task->start_time.tv_usec) / 1000;

                if (execution_time >= active_task->task.estimated_time) {
                    remove_active_task(i);
                }
            }
        }
    }
}

void start_server(const char *output_folder, int parallel_tasks, const char *sched_policy) {
    setup_communication(FIFO_PATH);
    printf("Servidor iniciado.\n");

    // Iniciar um processo para monitorar as tarefas ativas
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Este é o processo filho
        monitor_active_tasks();
        exit(EXIT_SUCCESS);
    }

    int fifo_fd = open(FIFO_PATH, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    ssize_t num_read;

    while (1) {
        // Verificar se há novas mensagens do cliente de forma assíncrona
        num_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        if (num_read == -1) {
            perror("read");
            continue;
        } else if (num_read == 0) {
            continue; // Nenhuma nova mensagem
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

        // Verificar se alguma tarefa ativa excedeu o tempo estimado
        for (int i = 0; i < active_count; i++) {
            ActiveTask *active_task = &active_tasks[i];
            struct timeval now;
            gettimeofday(&now, NULL);
            long execution_time = (now.tv_sec - active_task->start_time.tv_sec) * 1000 +
                                  (now.tv_usec - active_task->start_time.tv_usec) / 1000;
            if (execution_time >= active_task->task.estimated_time) {
                remove_active_task(i);
            }
        }
    }

    close(fifo_fd);
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s output_folder parallel_tasks sched_policy\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *output_folder = argv[1];
    int parallel_tasks = atoi(argv[2]);
    char *sched_policy = argv[3];

    // Verifique se output_folder é um diretório válido
    struct stat s;
    if (stat(output_folder, &s) != 0 || !S_ISDIR(s.st_mode)) {
        printf("Erro: %s não é um diretório válido.\n", output_folder);
        exit(EXIT_FAILURE);
    }

    // Verifique se parallel_tasks é um número válido
    if (parallel_tasks <= 0) {
        printf("Erro: parallel_tasks deve ser um número maior que 0.\n");
        exit(EXIT_FAILURE);
    }

    // Verifique se sched_policy é uma política de escalonamento válida
    if (strcmp(sched_policy, "FIFO") != 0 && strcmp(sched_policy, "RR") != 0) {
        printf("Erro: sched_policy deve ser 'FIFO' ou 'RR'.\n");
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
        printf("Erro: falha ao alocar memória para os arrays dinâmicos.\n");
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


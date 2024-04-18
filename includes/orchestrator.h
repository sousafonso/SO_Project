#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H
#define MAX_TASKS 100
#define MAX_PROGS_PER_TASK 10
#define COMMAND_LENGTH 4096


typedef struct {
    int id;
    pid_t pid;
    char commands[MAX_PROGS_PER_TASK][COMMAND_LENGTH];
    int num_commands;
    time_t start_time;
    time_t end_time;
    char status; // 'W' for waiting, 'R' for running, 'C' for complete
    int execution_time; // Real execution time
    int estimated_duration;
} Task;

void log_task_info(Task *task);

int parse_commands(char commands[MAX_PROGS_PER_TASK][COMMAND_LENGTH], const char* command_string); // ???

void execute_task(char *commands);

void handle_status_request();

int orchestrator(int argc, char *argv[]);

#endif /* ORCHESTRATOR_H */


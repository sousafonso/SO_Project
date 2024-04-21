#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H
#define MAX_TASKS 100
#define MAX_PROGS_PER_TASK 10
#define COMMAND_LENGTH 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

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

void handle_finished_task (int sig);

int parse_commands(char commands[MAX_PROGS_PER_TASK][COMMAND_LENGTH], const char* command_string); // ???

void execute_task(char *commands);

int execute_command(const char *command);

void handle_status_request();

void sort_tasks_by_estimated_duration(Task tasks[], int n);

int main(int argc, char *argv[]);

#endif /* ORCHESTRATOR_H */


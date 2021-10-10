#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "pa1.h"


const char *EVENT_LOG_FILE_NAME = "events_log";
const char *PIPE_LOG_FILE_NAME = "pipes_log";

typedef struct log_files {
    FILE *event_log;
    FILE *pipe_log;
};

struct pipe_table {
    size_t size;
    union custom_pipe **data;
};

union custom_pipe {
    int pipefds[2];
    struct {
        int read_fds;
        int write_fds;
    };
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// auxiliary functions
struct pipe_table create_pipe_table(size_t process_count) {
    struct pipe_table pipe_table;
    pipe_table.size = process_count;
    pipe_table.data = malloc(sizeof(union custom_pipe *) * process_count);
    for (size_t i = 0; i < process_count; i++) {
        pipe_table.data[i] = malloc(sizeof(union custom_pipe) * process_count);
    }
    return pipe_table;
}


void destroy_pipe_table(struct pipe_table pipe_table) {
    // TODO implement this
}

void print_line() {
    printf("\n");
}

void print_pipes_table(struct pipe_table pipe_table) {
    print_line();

    for (size_t i = 0; i < pipe_table.size; i++) {
        for (size_t j = 0; j < pipe_table.size; j++) {
            printf("r: %d, w: %d | ", pipe_table.data[i][j].read_fds, pipe_table.data[i][j].write_fds);
        }
        print_line();
    }

    print_line();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int create_pipes(struct pipe_table *pipe_table) {
    for (size_t i = 0; i < pipe_table->size; i++) {
        for (size_t j = 0; j < pipe_table->size; j++) {
            if (i == j) {
                pipe_table->data[i][j] = (union custom_pipe) {-1, -1};
            } else if (pipe(pipe_table->data[i][j].pipefds) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

void start_subprocess(size_t pid, size_t parent_id, struct pipe_table pipe_table, struct log_files log_files) {
    fprintf(log_files.event_log, log_started_fmt, pid, getpid(), parent_id);
    printf("LOG: SUBPROCESS with PID = %zu STARTED\n", pid);
    char message[] = "hello!";

    for (size_t j = 0; j < pipe_table.size; j++) {
        if (j == pid) {
            continue;
        }
        if (write(pipe_table.data[pid][j].write_fds, message, sizeof(message)) == -1) {
            fprintf(stderr, "LOG: SUBPROCESS with PID = %zu FAILED to SEND msg to %zu BY %d file\n", pid, j,
                    pipe_table.data[pid][j].write_fds);
        } else {
            printf("LOG: SUBPROCESS with PID = %zu SUCCESSFULLY SENT msg to %zu BY %d file\n", pid, j,
                   pipe_table.data[pid][j].write_fds);
        }
    }
    sleep(5);
    for (size_t j = 0; j < pipe_table.size; j++) {
        if (j == pid) {
            continue;
        }
        char *read_buffer = malloc(sizeof(message));


        if (read(pipe_table.data[j][pid].read_fds, read_buffer, sizeof(message)) == -1) {
            fprintf(stderr, "LOG: SUBPROCESS with PID = %zu FAILED to READ msg = %s to %zu BY %d file\n", pid,
                    read_buffer, j, pipe_table.data[j][pid].read_fds);
        } else {
            printf("LOG: SUBPROCESS with PID = %zu SUCCESSFULLY READ msg = %s to %zu BY %d file\n", pid, read_buffer, j,
                   pipe_table.data[j][pid].read_fds);
        }

    }
}

void start_parent_process(int pid, struct log_files log_files) {
    printf("LOG: PARENT PROCESS with pid = %d started\n", pid);
    sleep(10);
}

int create_subprocesses(struct pipe_table pipe_table, struct log_files log_files) {
    size_t pid_counter = 0;
    size_t parent_id = getpid();
    for (size_t i = 0; i < pipe_table.size; i++) {
        int fork_res = fork();
        if (fork_res == 0) {
            start_subprocess(pid_counter, parent_id, pipe_table, log_files);
            break;
        } else if (fork_res > 0) {
            // TODO probably that causes problems
            pid_counter++;
            if (i == pipe_table.size - 1) {
                start_parent_process(0, log_files);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

int open_log_files(struct log_files* log_files) {
    log_files->event_log = fopen(EVENT_LOG_FILE_NAME, "w");
    log_files->pipe_log = fopen(PIPE_LOG_FILE_NAME, "w");

    if (log_files->event_log == NULL || log_files->pipe_log == NULL) {
        return -1;
    } else {
        return 0;
    }
}

int close_log_files(struct log_files log_files) {
    if (fclose(log_files.event_log) != 0) {
        return -1;
    };
    if (fclose(log_files.pipe_log) != 0) {
        return -1;
    };
    return 0;
}

int main(int argc, char **argv) {
    size_t subprocess_count = atoi(argv[2]);
    if (subprocess_count == 0) {
        printf("LOG: process count is not specified manually\n");
        printf("LOG: default process count is 2\n");
        subprocess_count = 2;
    }

    struct pipe_table pipe_table = create_pipe_table(subprocess_count);

    if (create_pipes(&pipe_table) != 0) {
        perror("LOG: CREATION of a PIPE TABLE was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }


    if (create_subprocesses(pipe_table, log_files) != 0) {
        perror("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    destroy_pipe_table(pipe_table);
    // TODO add closing log files
    return 0;
}

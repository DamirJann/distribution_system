#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "pa1.h"
#include "ipc.h"


const char *EVENT_LOG_FILE_NAME = "events_log";
const char *PIPE_LOG_FILE_NAME = "pipes_log";

struct log_files {
    FILE *event_log;
    FILE *pipe_log;
};

struct pipe_table {
    local_id size;
    union custom_pipe **data;
};

union custom_pipe {
    int pipefds[2];
    struct {
        int read_fds;
        int write_fds;
    };
};

struct send_info{
    local_id from_local_pid;
    struct pipe_table pipe_table;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// auxiliary functions
struct pipe_table create_pipe_table(local_id process_count) {
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

void print_pipes_table(FILE* file, struct pipe_table pipe_table) {
    fprintf(file, "\n");

    for (size_t i = 0; i < pipe_table.size; i++) {
        for (size_t j = 0; j < pipe_table.size; j++) {
            fprintf(file, "r: %d, w: %d | ", pipe_table.data[i][j].read_fds, pipe_table.data[i][j].write_fds);
        }
        fprintf(file, "\n");
    }

    fprintf(file, "\n");
}

int open_log_files(struct log_files *log_files) {
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int send_multicast(void *self, const Message *msg) {
    struct send_info send_info = *(struct send_info*) self;

    for (local_id i = 0; i < send_info.pipe_table.size; i++) {
        if (i != send_info.from_local_pid) {
            if (send(self, i, msg) == -1) {
                fprintf(stderr, "LOG [%d]: FAILED to SENT MULTICAST msg\n", send_info.from_local_pid);
                return -1;
            }
        }
    }
    printf("LOG [%d]: SUCCESSFULLY SENT MULTICAST msg\n", send_info.from_local_pid);
    return 0;
}

int send(void *self, local_id dst, const Message *msg) {
    struct send_info send_info = *(struct send_info*) self;
    int fd = send_info.pipe_table.data[send_info.from_local_pid][dst].write_fds;

    if (write(fd, msg, sizeof(*msg)) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to SENT msg to %hhd BY %d file\n", send_info.from_local_pid, dst, fd);
        return -1;
    } else{
        printf("LOG [%d]: SUCCESSFULLY SENT msg to %hhd BY %d file\n", send_info.from_local_pid, dst, fd);
        return 0;
    }
}

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


void start_subprocess(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files) {
    fprintf(log_files.event_log, log_started_fmt, local_pid, getpid(), parent_pid);
    fprintf(stdin, log_started_fmt, local_pid, getpid(), parent_pid);

    const Message message;

    send_multicast((void *) &(struct send_info){local_pid, pipe_table}, &message);


    // TODO implement logic of receiving
    fprintf(log_files.event_log, log_received_all_started_fmt, local_pid);
    fprintf(log_files.event_log, log_done_fmt, local_pid);
    fprintf(log_files.event_log, log_received_all_done_fmt, local_pid);

    sleep(5);

}

void start_parent_process(local_id local_pid, struct log_files log_files) {
    printf("LOG: PARENT PROCESS with local_pid = %d started\n", local_pid);
    sleep(10);
    if (close_log_files(log_files) == -1) {
        fprintf(stderr, "LOG: FAILED to CLOSE log files");
        exit(EXIT_FAILURE);
    };
}

int create_subprocesses(struct pipe_table pipe_table, struct log_files log_files) {
    local_id local_pid_counter = 0;
    int parent_pid = getpid();
    for (local_id i = 0; i < pipe_table.size; i++) {
        int fork_res = fork();
        if (fork_res == 0) {
            start_subprocess(local_pid_counter, parent_pid, pipe_table, log_files);
            break;
        } else if (fork_res > 0) {
            // TODO probably that causes problems
            local_pid_counter++;
            if (i == pipe_table.size - 1) {
                start_parent_process(0, log_files);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    local_id subprocess_count = atoi(argv[2]);
    if (subprocess_count == 0) {
        printf("LOG: process count is not specified manually\n");
        printf("LOG: default process count is 2\n");
        subprocess_count = 3;
    }
    printf("hello");

    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }

    struct pipe_table pipe_table = create_pipe_table(subprocess_count);

    print_pipes_table(log_files.pipe_log, pipe_table);

    if (create_pipes(&pipe_table) != 0) {
        perror("LOG: CREATION of a PIPE TABLE was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }



    if (create_subprocesses(pipe_table, log_files) != 0) {
        perror("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    destroy_pipe_table(pipe_table);
    // TODO add closing log files
    return 0;
}

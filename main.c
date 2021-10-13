#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "pa1.h"
#include "ipc.h"

const char *EVENT_LOG_FILE_NAME = "events_log";
const char *PIPE_LOG_FILE_NAME = "pipes_log";

struct log_files {
    FILE *event_log;
    FILE *pipe_log;
};


struct custom_pipe {
    int read_fds;
    int write_fds;
};

struct pipe_table {
    local_id size;
    struct custom_pipe **data;
};


struct process_info {
    local_id id;
    struct pipe_table pipe_table;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// auxiliary functions
struct pipe_table create_pipe_table(local_id process_count) {
    struct pipe_table pipe_table;
    pipe_table.size = process_count;
    pipe_table.data = malloc(sizeof(struct custom_pipe *) * process_count);
    for (size_t i = 0; i < process_count; i++) {
        pipe_table.data[i] = malloc(sizeof(struct custom_pipe) * process_count);
    }
    return pipe_table;
}

void destroy_pipe_table(struct pipe_table pipe_table) {
    for (local_id i = 0; i < pipe_table.size; i++){
        free(pipe_table.data[i]);
    }
    free(pipe_table.data);
}

void print_pipes_table(FILE *file, struct pipe_table pipe_table) {
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
    }
    if (fclose(log_files.pipe_log) != 0) {
        return -1;
    }
    return 0;
}

Message create_start_message(){
    return (Message) {
        .s_header.s_type = STARTED,
        .s_header.s_local_time = (int16_t)time(NULL),
        .s_header.s_magic = MESSAGE_MAGIC,
        .s_header.s_payload_len = 0,
    };
}

Message create_done_message(){
    return (Message) {
            .s_header.s_type = DONE,
            .s_header.s_local_time = (int16_t)time(NULL),
            .s_header.s_magic = MESSAGE_MAGIC,
            .s_header.s_payload_len = 0,
    };
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int send_multicast(void *self, const Message *msg) {
    struct pipe_table pipe_table = (*(struct process_info *) self).pipe_table;
    local_id from = (*(struct process_info *) self).id;

    for (local_id i = 0; i < pipe_table.size; i++) {
        if (i != from) {
            if (send(self, i, msg) == -1) {
                fprintf(stderr, "LOG [%d]: FAILED to SENT MULTICAST msg\n", from);
                return -1;
            }
        }
    }

    return 0;
}

int send(void *self, local_id dst, const Message *msg) {
    struct pipe_table pipe_table = (*(struct process_info *) self).pipe_table;
    local_id from = (*(struct process_info *) self).id;

    int fd = pipe_table.data[from][dst].write_fds;

    if (write(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to SENT msg = %hd to %hhd BY %d file\n", from,
                msg->s_header.s_type, dst, fd);
        return -1;
    } else {
        printf("LOG [%d]: SUCCESSFULLY SENT msg = %hd to %hhd BY %d file\n", from, msg->s_header.s_type,
               dst, fd);
        return 0;
    }
}

int receive_any(void *self, Message *msg) {
    struct pipe_table pipe_table = (*(struct process_info *) self).pipe_table;
    local_id to = (*(struct process_info *) self).id;

    for (local_id i = 1; i < pipe_table.size; i++) {
        if (i != to) {
            if (receive(self, i, msg) == -1) {
                fprintf(stderr, "LOG [%d]: FAILED to RECEIVE MULTICAST msg = %hd\n", to,
                        msg->s_header.s_type);
                return -1;
            }
        }
    }
    return 0;
}


int receive(void *self, local_id from, Message *msg) {
    struct pipe_table pipe_table = (*(struct process_info *) self).pipe_table;
    local_id to = (*(struct process_info *) self).id;

    int fd = pipe_table.data[from][to].read_fds;

    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to RECEIVE msg = %hd from %hhd BY %d file\n", to,
                msg->s_header.s_type, from, fd);
        return -1;
    } else {
        printf("LOG [%d]: SUCCESSFULLY RECEIVED msg = %hd  from %hhd BY %d file\n", to,
               msg->s_header.s_type, from, fd);
        return 0;
    }
}

int create_pipes(struct pipe_table *pipe_table) {
    for (size_t i = 0; i < pipe_table->size; i++) {
        for (size_t j = 0; j < pipe_table->size; j++) {

            if (i == j) {
                continue;
            }

            int pipefds[2] = {-1, -1};
            if (pipe(pipefds) == -1) {
                return -1;
            }

            pipe_table->data[i][j].read_fds = pipefds[0];
            pipe_table->data[i][j].write_fds = pipefds[1];

        }
    }

    return 0;
}

void start_work() {
    // DO NOTHING
}

void start_subprocess(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files) {
    fprintf(log_files.event_log, log_started_fmt, local_pid, getpid(), parent_pid);
    fprintf(stdin, log_started_fmt, local_pid, getpid(), parent_pid);

    Message start_message = create_start_message();


    send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &start_message);

    if (pipe_table.size > 2) {
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &start_message);
        fprintf(log_files.event_log, log_received_all_started_fmt, local_pid);
        fprintf(stdin, log_received_all_started_fmt, local_pid);
    }


    start_work();

    fprintf(log_files.event_log, log_done_fmt, local_pid);
    fprintf(stdin, log_done_fmt, local_pid);


    Message done_message = create_done_message();
    send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &done_message);

    if (pipe_table.size > 2) {
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &done_message);
        fprintf(log_files.event_log, log_received_all_done_fmt, local_pid);
        fprintf(stdin, log_received_all_done_fmt, local_pid);
    }


}

void start_parent_process(local_id local_pid, struct log_files log_files, struct pipe_table pipe_table) {
    printf("LOG: PARENT PROCESS with local_pid = %d started\n", local_pid);

    Message start_message = create_start_message();
    receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &start_message);

    Message done_message = create_done_message();
    receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &done_message);
    if (close_log_files(log_files) == -1) {
        fprintf(stderr, "LOG: FAILED to CLOSE log files");
        exit(EXIT_FAILURE);
    }
    destroy_pipe_table(pipe_table);
}

int create_subprocesses(struct pipe_table pipe_table, struct log_files log_files) {

    int parent_pid = getpid();
    for (local_id i = 1; i < pipe_table.size; i++) {
        int fork_res = fork();
        if (fork_res == 0) {
            start_subprocess(i, parent_pid, pipe_table, log_files);
            break;
        } else if (fork_res > 0) {
            // TODO probably that causes problems
            if (i == pipe_table.size - 1) {
                start_parent_process(0, log_files, pipe_table);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    local_id subprocess_count = (local_id) strtol(argv[2], NULL, 10);
    if (subprocess_count == 0) {
        printf("LOG: process count is not specified manually\n");
        printf("LOG: default process count is 2\n");
        subprocess_count = 2;
    }

    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }

    struct pipe_table pipe_table = create_pipe_table((local_id) (subprocess_count + 1));

    if (create_pipes(&pipe_table) != 0) {
        perror("LOG: CREATION of a PIPE TABLE was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    print_pipes_table(log_files.pipe_log, pipe_table);

    if (create_subprocesses(pipe_table, log_files) != 0) {
        perror("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

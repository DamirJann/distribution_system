#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "auxiliary.h"
#include "process.h"
#include "main_process.h"
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

int send_multicast(void *self, const Message *msg) {
    fprintf(stdout, "START SEND MULTICAST\n");
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    for (local_id i = 0; i < pipe_table.size; i++) {
        if (i == from) {
            continue;
        }
        send(self, i, msg);
    }
    return 0;
}

int send(void *self, local_id dst, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][dst].write_fds;

    if (write(fd, msg, msg->s_header.s_payload_len + sizeof(MessageHeader)) == -1) {
        fprintf(stderr, "%d: process %d FAILED to SENT msg = %s to %hhd\n", get_lamport_time(), from,
                msg->s_payload, dst);
        return -1;
    } else {
        printf("%d: process %d  SUCCESSFULLY SENT msg('%s', %s) to %hhd process\n", get_lamport_time(), from, msg->s_payload, messageTypes[msg->s_header.s_type], dst);
        return 0;
    }
}

int receive(void *self, local_id from, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id to = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][to].read_fds;
    if (read(fd, &msg->s_header, sizeof(MessageHeader)) < 1) {
//        fprintf(stderr, "LOG [%d]: FAILED to RECEIVE HEADER from %hhd process by %d file\n", to, from, fd);
        return -1;
    }

    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "%d: %d FAILED to RECEIVE PAYLOAD from %hhd process\n", get_lamport_time(), to, from);
        return -1;
    }

    printf("%d: process %d SUCCESSFULLY RECEIVED msg('%s', %s) from %hhd process\n", get_lamport_time(), to, msg->s_payload, messageTypes[msg->s_header.s_type], from);
    return 0;
}

int receive_any(void *self, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    while (1) {
        for (local_id i = 0; i < pipe_table.size; i++) {
            if (receive(self, i, msg) == 0) {
                return 0;
            }
        }
    }
}

int create_pipes(struct pipe_table *pipe_table) {
    for (size_t i = 0; i < pipe_table->size; i++) {
        for (size_t j = 0; j < pipe_table->size; j++) {

            if (i == j) {
                pipe_table->data[i][j].read_fds = -1;
                pipe_table->data[i][j].write_fds = -1;
                continue;
            }

            int pipefds[2] = {-1, -1};
            if (pipe(pipefds) == -1) {
                return -1;
            }

            pipe_table->data[i][j].read_fds = pipefds[0];
            pipe_table->data[i][j].write_fds = pipefds[1];

            fcntl(pipe_table->data[i][j].read_fds, F_SETFL, O_NONBLOCK);
            fcntl(pipe_table->data[i][j].write_fds, F_SETFL, O_NONBLOCK);

        }
    }

    return 0;
}

int create_subprocesses(struct pipe_table pipe_table, struct log_files log_files) {
    int parent_pid = getpid();
    for (local_id i = 1; i < pipe_table.size; i++) {
        int fork_res = fork();
        if (fork_res == 0) {
            start_process(i, parent_pid, pipe_table, log_files);
            break;
        } else if (fork_res > 0) {
            if (i == pipe_table.size - 1) {
                start_main_process(PARENT_ID, log_files, pipe_table);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

void parse_argc(int argc, char **argv, local_id *process_count, bool *is_mutex_mode) {
    if (getopt(argc, argv, "p") == -1) {
        perror("LOG: FAILED to PARSE p parameter");
        exit(EXIT_FAILURE);
    }
    *process_count = (local_id) strtol(argv[optind], NULL, 10);

    if (getopt(argc, argv, "mutexl") == -1){
        *is_mutex_mode = false;
    } else{
        *is_mutex_mode = true;
    }
}

int main(int argc, char **argv) {

    local_id process_count;
    bool is_mutex_mode;

    parse_argc(argc, argv, &process_count, &is_mutex_mode);
    printf("%d, %d\n",process_count, is_mutex_mode);

    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }

    struct pipe_table pipe_table = create_pipe_table((local_id) (process_count + 1));
    if (create_pipes(&pipe_table) != 0) {
        perror("LOG: CREATION of a PIPE TABLE was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    if (create_subprocesses(pipe_table, log_files) != 0) {
        perror("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    return 0;

}

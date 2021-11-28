#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "banking.h"
#include "auxiliary.h"
#include "c_process.h"
#include "k_process.h"
#include <fcntl.h>
#include <getopt.h>
#include <string.h>

int send_multicast(void *self, const Message *msg) {
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
        return 0;
    }
}

int receive(void *self, local_id from, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id to = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][to].read_fds;
    if (read(fd, &msg->s_header, sizeof(MessageHeader)) < 1) {
        return -1;
    }

    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "%d: %d FAILED to RECEIVE PAYLOAD from %hhd process\n", get_lamport_time(), to, from);
        return -1;
    }

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

int create_subprocesses(struct pipe_table pipe_table, struct log_files log_files, balance_t *init_balance) {
    int parent_pid = getpid();
    for (local_id i = 1; i < pipe_table.size; i++) {
        int fork_res = fork();
        if (fork_res == 0) {
            start_c_subprocess(i, parent_pid, pipe_table, log_files, init_balance[i - 1]);
            break;
        } else if (fork_res > 0) {
            if (i == pipe_table.size - 1) {
                start_k_process(PARENT_ID, log_files, pipe_table);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {


    if (getopt(argc, argv, "p") == -1) {
        perror("LOG: FAILED to PARSE p parameter");
        exit(EXIT_FAILURE);
    }

    local_id c_subprocess_count = (local_id) strtol(argv[optind], NULL, 10);

    balance_t *init_balance = create_balance_t_array(c_subprocess_count);

    for (int i = 0; i < c_subprocess_count; i++) {
        init_balance[i] = (balance_t) strtol(argv[i + optind + 1], NULL, 10);
    }

    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }

    struct pipe_table pipe_table = create_pipe_table((local_id) (c_subprocess_count + 1));
    if (create_pipes(&pipe_table) != 0) {
        perror("LOG: CREATION of a PIPE TABLE was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    if (create_subprocesses(pipe_table, log_files, init_balance) != 0) {
        perror("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    destroy_balance_t_array(init_balance);

    return 0;
}

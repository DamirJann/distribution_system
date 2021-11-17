#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>
#include "pa2345.h"
#include "ipc.h"
#include "banking.h"
#include "auxiliary.h"


int send_multicast(void *self, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    for (local_id i = 0; i < pipe_table.size; i++) {
        if (i == from) {
            continue;
        }

        if (send(self, i, msg) == -1) {
            fprintf(stderr, "LOG [%d]: FAILED to SENT MULTICAST msg\n", from);
            return -1;
        }

    }

    return 0;
}

int send(void *self, local_id dst, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    printf("%s - %d\n", msg->s_payload, msg->s_header.s_payload_len);

    int fd = pipe_table.data[from][dst].write_fds;

    if (write(fd, msg, msg->s_header.s_payload_len + sizeof(MessageHeader)) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to SENT msg = %s to %hhd BY %d file\n", from,
                msg->s_payload, dst, fd);
        return -1;
    } else {
        printf("LOG [%d]: SUCCESSFULLY SENT msg = %s to %hhd BY %d file\n", from, msg->s_payload,
               dst, fd);
        return 0;
    }
}

int receive_from_all(struct process_info process_info, Message *msg) {

    for (local_id i = 1; i < process_info.pipe_table.size; i++) {
        if (i == process_info.id) {
            continue;
        }

        if (receive((void *) &process_info, i, msg) == -1) {
            fprintf(stderr, "LOG [%d]: FAILED to RECEIVE MULTICAST msg = %s\n", process_info.id,
                    msg->s_payload);
            return -1;
        }
    }
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    struct pipe_table pipe_table = (*(struct process_info *) self).pipe_table;
    local_id to = (*(struct process_info *) self).id;
    int fd = pipe_table.data[from][to].read_fds;

    if (read(fd, &msg->s_header, sizeof(MessageHeader)) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to RECEIVE HEADER from %hhd BY %d file\n", to, from, fd);
        return -1;
    }

    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to RECEIVE PAYLOAD from %hhd BY %d file\n", to, from, fd);
        return -1;
    }

    printf("LOG [%d]: SUCCESSFULLY RECEIVED msg = %s  from %hhd BY %d file\n", to, msg->s_payload, from, fd);
    return 0;
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

        }
    }

    return 0;
}

void start_subprocess_work() {
    // DO NOTHING
}

void start_subprocess(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);

    fprintf(log_files.event_log, log_started_fmt, local_pid, getpid(), parent_pid);
    fprintf(stdin, log_started_fmt, local_pid, getpid(), parent_pid);

    Message start_message = create_start_message();
    start_message.s_header.s_payload_len = snprintf(start_message.s_payload, MAX_PAYLOAD_LEN, log_started_fmt,
                                                    local_pid, getpid(), parent_pid);

    send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &start_message);
    receive_from_all((struct process_info) {local_pid, pipe_table}, &start_message);

    fprintf(log_files.event_log, log_received_all_started_fmt, local_pid);
    fprintf(stdin, log_received_all_started_fmt, local_pid);

    start_subprocess_work();

    fprintf(log_files.event_log, log_done_fmt, local_pid);
    fprintf(stdin, log_done_fmt, local_pid);


    Message done_message = create_done_message();
    done_message.s_header.s_payload_len = sprintf(done_message.s_payload, log_done_fmt, local_pid);

    send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &done_message);
    receive_from_all((struct process_info) {local_pid, pipe_table}, &done_message);

    fprintf(log_files.event_log, log_received_all_done_fmt, local_pid);
    fprintf(stdin, log_received_all_done_fmt, local_pid);
}

void wait_subprocesses(local_id process_count) {
    for (local_id i = 0; i < process_count; i++) {
        if (wait(NULL) == -1) {
            fprintf(stderr, "LOG: FAILED to WAIT all subprocesses");
            exit(EXIT_FAILURE);
        }
    }

    if (wait(NULL) != -1) {
        fprintf(stderr, "LOG: NOT all subprocesses were waited");
        exit(EXIT_FAILURE);
    }
    fprintf(stdin, "LOG: SUCCESSFULLY WAITED all subprocesses");
}

void start_parent_process(local_id local_pid, struct log_files log_files, struct pipe_table pipe_table) {
    printf("LOG: PARENT PROCESS with local_pid = %d started\n", local_pid);
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);

    Message start_message = create_start_message();
    receive_from_all((struct process_info) {local_pid, pipe_table}, &start_message);

    Message done_message = create_done_message();
    receive_from_all((struct process_info) {local_pid, pipe_table}, &done_message);

    wait_subprocesses((local_id) (pipe_table.size - 1));

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
            if (i == pipe_table.size - 1) {
                start_parent_process(PARENT_ID, log_files, pipe_table);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount)
{
    // TODO student, please implement me
}


int main(int argc, char **argv) {
    local_id subprocess_count = (local_id) strtol(argv[2], NULL, 10);

    printf("LOG: process count is %hhd\n", subprocess_count);


    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }

    struct pipe_table pipe_table = create_pipe_table((local_id) (subprocess_count + 1));

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

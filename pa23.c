#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <wait.h>
#include "pa2345.h"
#include "ipc.h"
#include "banking.h"
#include "auxiliary.h"
#include<fcntl.h>
#include<getopt.h>
#include <string.h>
#include <assert.h>
#include <expat.h>

int send_multicast(void *self, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    for (local_id i = 0; i < pipe_table.size; i++) {
        if (i == from) {
            continue;
        }

        if (send(self, i, msg) == -1) {
            return -1;
        }

    }
    return 0;
}

int send(void *self, local_id dst, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][dst].write_fds;

    if (write(fd, msg, msg->s_header.s_payload_len + sizeof(MessageHeader)) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to SENT msg = %s to %hhd\n", from,
                msg->s_payload, dst);
        return -1;
    } else {
//        printf("LOG [%d]: SUCCESSFULLY SENT msg('%s') to %hhd process\n", from, msg->s_payload, dst);
        return 0;
    }
}

int receive(void *self, local_id from, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id to = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][to].read_fds;
    if (read(fd, &msg->s_header, sizeof(MessageHeader)) == -1) {
//        fprintf(stderr, "LOG [%d]: FAILED to RECEIVE HEADER from %hhd process by %d file\n", to, from, fd);
        return -1;
    }

    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "LOG [%d]: FAILED to RECEIVE PAYLOAD from %hhd process\n", to, from);
        return -1;
    }

//    printf("LOG [%d]: SUCCESSFULLY RECEIVED msg('%s') from %hhd process\n", to, msg->s_payload, from);
    return 0;
}

int receive_any(void *self, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    while (1) {
        for (local_id i = 0; i < pipe_table.size; i++) {
            if (receive(self, i, msg) != -1) {
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

TransferOrder parse_transfer_order(const char *msg) {
    TransferOrder transfer_order;
    char *s = malloc(sizeof(char) * MAX_PAYLOAD_LEN);
    strncpy(s, msg, MAX_PAYLOAD_LEN);

    char *src = strtok(s, ",");
    transfer_order.s_src = (local_id) strtol(src, NULL, 10);


    char *dst = strtok(NULL, ",");
    transfer_order.s_dst = (local_id) strtol(dst, NULL, 10);

    char *amount = strtok(NULL, ",");
    transfer_order.s_amount = (balance_t) strtol(amount, NULL, 10);
    return transfer_order;
}

void handle_transfer_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                             balance_t *init_balance, const Message received_msg) {

    const TransferOrder transfer_order = parse_transfer_order(received_msg.s_payload);

    if (transfer_order.s_src == local_pid) {
        *init_balance -= transfer_order.s_amount;
        send((void *) &(struct process_info) {local_pid, pipe_table}, transfer_order.s_dst, &received_msg);
        fprintf(log_files.event_log, log_transfer_out_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_dst );
        fprintf(stdout, log_transfer_out_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_dst );
    } else {
        *init_balance += transfer_order.s_amount;;
        Message ack_msg = (Message) {
                .s_header.s_type = ACK,
                .s_header.s_local_time = get_physical_time(),
                .s_header.s_magic = MESSAGE_MAGIC,
                // write message to payload and its size to payload_len
                .s_header.s_payload_len = 0
        };
        send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &ack_msg);
        fprintf(log_files.event_log, log_transfer_in_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_src);
        fprintf(stdout, log_transfer_in_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_src);
    }


}

void addNewBalanceState(BalanceHistory *balance_history, BalanceState balance_state) {
    balance_history->s_history[balance_history->s_history_len] = balance_state;
    balance_history->s_history_len++;
}

void start_subprocess_work(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                           balance_t init_balance) {
    fprintf(stdout, "LOG[%d]: SUCCESSFULLY STARTED SUBPROCESS WORK\n", local_pid);

    Message msg;
    BalanceHistory balanceHistory = (BalanceHistory) {.s_id = local_pid, .s_history_len = 0};

    addNewBalanceState(&balanceHistory, (BalanceState) {
            .s_balance = init_balance,
            .s_time = get_physical_time(),
            .s_balance_pending_in = 0,
    });

    balance_t current_balance = init_balance;
    while (1) {
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);

        switch (msg.s_header.s_type) {
            case DONE: {
                fprintf(log_files.event_log, log_done_fmt, get_physical_time(), local_pid, current_balance);
                fprintf(stdout, log_done_fmt, get_physical_time(), local_pid, current_balance);
                return;
            }
            case TRANSFER: {
                handle_transfer_message(local_pid, pipe_table, log_files, &current_balance, msg);
                break;
            }
            default: {
                fprintf(stdout, "LOG[%d]: got message with UNKNOWN type during WORK step\n", local_pid);
                break;
            }
        }

        addNewBalanceState(&balanceHistory, (BalanceState) {
                .s_balance = current_balance,
                .s_time = get_physical_time(),
                .s_balance_pending_in = 0,
        });
    }

}


void start_c_subprocess(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files,
                        balance_t init_balance) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);

    fprintf(log_files.event_log, log_started_fmt, get_physical_time(), local_pid, getpid(), parent_pid, init_balance);
    fprintf(stdout, log_started_fmt, get_physical_time(), local_pid, getpid(), parent_pid, init_balance);

    Message start_message = (Message) {
            .s_header.s_type = STARTED,
            .s_header.s_local_time = get_physical_time(),
            .s_header.s_magic = MESSAGE_MAGIC,
            // write message to payload and its size to payload_len
            .s_header.s_payload_len = snprintf(start_message.s_payload, MAX_PAYLOAD_LEN, log_started_fmt,
                                               get_physical_time(), local_pid, getpid(), parent_pid, init_balance)

    };


    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &start_message);
    start_subprocess_work(local_pid, pipe_table, log_files, init_balance);

}

void wait_subprocesses(local_id process_count) {
    for (local_id i = 0; i < process_count; i++) {
        if (wait(NULL) == -1) {
            fprintf(stderr, "LOG: FAILED to WAIT all subprocesses\n");
            exit(EXIT_FAILURE);
        }
    }

    if (wait(NULL) != -1) {
        fprintf(stderr, "LOG: NOT all subprocesses were waited\n");
        exit(EXIT_FAILURE);
    }
    printf("LOG: SUCCESSFULLY WAITED all subprocesses\n");
}

void receive_started_message_from_all(local_id to, struct pipe_table pipe_table) {
    local_id received_started_msg_count = 0;
    while (received_started_msg_count != pipe_table.size - 1) {
        Message msg;
        int res = receive_any((void *) &(struct process_info) {to, pipe_table}, &msg);
        if (res == 0 && msg.s_header.s_type == STARTED) {
            received_started_msg_count++;
        }
    }
}

Message create_done_message(){
    return (Message) {
            .s_header.s_type = DONE,
            .s_header.s_local_time = get_physical_time(),
            .s_header.s_magic = MESSAGE_MAGIC,
            // write message to payload and its size to payload_len
            .s_header.s_payload_len = 0
    };
}

void start_k_process(local_id local_pid, struct log_files log_files, struct pipe_table pipe_table) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);


    receive_started_message_from_all(PARENT_ID, pipe_table);
    fprintf(log_files.event_log, log_received_all_started_fmt, get_physical_time(), local_pid);
    fprintf(stdout, log_received_all_started_fmt, get_physical_time(), local_pid);

    bank_robbery((void *) &pipe_table, pipe_table.size - 1);


    Message message = create_done_message();
    if (send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &message) == -1){
        fprintf(stderr, "LOG[%d]: FAILED to SEND multicast DONE message");
    } else {
        fprintf(log_files.event_log, log_received_all_done_fmt, get_physical_time(), local_pid);
        fprintf(stdout, log_received_all_done_fmt, get_physical_time(), local_pid);
    }

    if (close_log_files(log_files) == -1) {
        fprintf(stderr, "LOG: FAILED to CLOSE log files\n");
        exit(EXIT_FAILURE);
    }
    destroy_pipe_table(pipe_table);
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

void transfer(void *parent_data, local_id src, local_id dst,
              balance_t amount) {
    struct pipe_table pipe_table = *((struct pipe_table *) parent_data);
    struct process_info process_info = (struct process_info) {PARENT_ID, pipe_table};

    Message transfer_msg = (Message) {
            .s_header.s_type = TRANSFER,
            .s_header.s_local_time = get_physical_time(),
            .s_header.s_magic = MESSAGE_MAGIC,
            // write message to payload and its size to payload_len
            .s_header.s_payload_len = snprintf(transfer_msg.s_payload, MAX_PAYLOAD_LEN,
                                               "%d, %d, %d,", src, dst, amount)
    };
    send((void *) &process_info, src, &transfer_msg);
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

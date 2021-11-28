//
// Created by damire on 14.10.2021.
//

#include <string.h>
#include "banking.h"

#ifndef LAB1_AUXILIARY_H
#define LAB1_AUXILIARY_H

#endif //LAB1_AUXILIARY_H

void increase_lamport_counter();
void sync_lamport_time_with_another_process(timestamp_t);

typedef enum {
    DEBUG,
    INFO
} Log_level;

struct log_files {
    Log_level log_level;
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

struct transfer_info {
    struct pipe_table pipe_table;
    local_id recipient;
    balance_t amount;
};


struct process_info {
    local_id id;
    struct pipe_table pipe_table;
};

static const char *const log_received_all_ack_fmt =
        "%d: process %1d received all ACK messages\n";

static const char *const log_process_finished =
        "%d: process %d finished\n";

static const char *messageTypes[] = {
        [STARTED] = "STARTED",
        [DONE] = "DONE",
        [ACK] = "ACK",
        [STOP] = "STOP",
        [TRANSFER] = "TRANSFER",
        [BALANCE_HISTORY] = "BALANCE_HISTORY",
        [CS_REQUEST] = "CS_REQUEST",
};

TransferOrder retrieve_from_message(Message);

int blocked_receive(void *, local_id, Message *);

Message create_default_message(MessageType type);

void close_extra_pipe_by_subprocess(local_id, struct pipe_table *);

struct pipe_table create_pipe_table(local_id);

void destroy_pipe_table(struct pipe_table);

void print_pipes_table(local_id, FILE *, struct pipe_table);

int open_log_files(struct log_files *);

int close_log_files(struct log_files);

balance_t *create_balance_t_array(local_id);

void destroy_balance_t_array(balance_t *);

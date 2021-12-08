//
// Created by damire on 14.10.2021.
//

#include <string.h>
#include <stdbool.h>
#include "ipc.h"

#ifndef LAB1_AUXILIARY_H
#define LAB1_AUXILIARY_H

#endif //LAB1_AUXILIARY_H

timestamp_t get_lamport_time();

void increase_lamport_counter();

void sync_lamport_time_after_receiving(timestamp_t incoming_msg);

void sync_lamport_time_before_sending();

static const char *const replicated_queue_log = "replicated_queue.log";

typedef enum {
    DEBUG,
    INFO
} Log_level;

struct process_request {
    timestamp_t lamport_time;
    local_id process_id;
};

struct queue_elem {
    struct process_request process_request;
    struct queue_elem *next;
};

struct replicated_queue {
    struct queue_elem *head;
    local_id size;
};


struct queue_elem *create_queue_elem(struct process_request );

struct replicated_queue create_replicated_queue();

struct process_request front(struct replicated_queue);

bool is_in_head(struct replicated_queue queue, local_id local_pid);

void pop(struct replicated_queue *);

void push(struct replicated_queue *, struct process_request);

void print_replicated_queue(FILE *file, struct replicated_queue);

void destroy_replicated_queue(struct replicated_queue *);

struct process_request retrieve_from_message(Message);

void write_payload(Message *, struct process_request);


int blocked_receive(void *self, local_id from, Message *msg);

struct log_files {
    Log_level log_level;
    FILE *event_log;
    FILE *pipe_log;
    FILE *replicated_queue_log;
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
        [CS_REPLY] = "CS_REPLY",
        [CS_RELEASE] = "CS_RELEASE",
};


Message create_default_message(MessageType type);

void close_extra_pipe_by_subprocess(local_id, struct pipe_table *);

struct pipe_table create_pipe_table(local_id);

void destroy_pipe_table(struct pipe_table);

void print_pipes_table(local_id, FILE *, struct pipe_table);

int open_log_files(struct log_files *);

int close_log_files(struct log_files);

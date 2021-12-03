#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "utils.h"
#include "common.h"

timestamp_t current_lamport_time = 0;

void increase_lamport_counter() {
    current_lamport_time++;
}

void sync_lamport_time_after_receiving(timestamp_t incoming_msg) {
    if (incoming_msg > current_lamport_time) {
        current_lamport_time = incoming_msg;
    }
    increase_lamport_counter();
}

void sync_lamport_time_before_sending() {
    increase_lamport_counter();
}


struct process_request retrieve_from_message(Message message){
    struct process_request r;
    memcpy((void*) &r, (void*) message.s_payload, message.s_header.s_payload_len);
    return r;
}

struct queue_elem *create_queue_elem(struct process_request new_process_request) {
    struct queue_elem *new_elem = malloc(sizeof(struct queue_elem));
    new_elem->next = NULL;
    new_elem->process_request = new_process_request;
    return new_elem;
}

void write_payload(Message* msg, struct process_request request){
    msg->s_header.s_payload_len = sizeof(request);
    memcpy((void *) msg->s_payload, (void *) &request, msg->s_header.s_payload_len);
}

struct replicated_queue create_replicated_queue() {
    return (struct replicated_queue) {
            .head = create_queue_elem((struct process_request){.process_id = -1, .lamport_time = -1}),
            .size = 0
    };
}

int cmp(const struct queue_elem *l, const struct queue_elem *r) {
    if (l->process_request.lamport_time < r->process_request.lamport_time) return -1;
    if (l->process_request.lamport_time > r->process_request.lamport_time) return 1;

    if (l->process_request.process_id < r->process_request.process_id) return -1;
    if (l->process_request.process_id > r->process_request.process_id) return 1;

    return 0;
}

void print_replicated_queue(FILE * file, struct replicated_queue replicated_queue){
    struct queue_elem* curr_elem = replicated_queue.head;
    while (curr_elem != NULL){
        fprintf(file, "(%d, %d) | ", curr_elem->process_request.lamport_time, curr_elem->process_request.process_id);
        curr_elem = curr_elem->next;
    }
    fprintf(file, "\n");
}

struct process_request front(struct replicated_queue queue){
    return queue.head->next->process_request;
}

void push(struct replicated_queue* queue, struct process_request new_process_request) {
    struct queue_elem* new_elem = create_queue_elem(new_process_request);

    struct queue_elem* curr_elem = queue->head;
    while (curr_elem->next != NULL && cmp(curr_elem->next, new_elem) < 0){
        curr_elem = curr_elem->next;
    }
    new_elem->next = curr_elem->next;
    curr_elem->next = new_elem;
    queue->size++;
}

void pop(struct replicated_queue *queue) {
    struct queue_elem *deleted_elem = queue->head->next;
    queue->head->next = deleted_elem->next;
    queue->size--;
    free(deleted_elem);
}

void destroy_replicated_queue(struct replicated_queue* queue){
    while (queue->size != 0){
        pop(queue);
    }
}

timestamp_t get_lamport_time() {
    return current_lamport_time;
}

int blocked_receive(void *self, local_id from, Message *msg) {
    while (receive(self, from, msg) != 0);
    return 0;
}

Message create_default_message(MessageType type) {
    return (Message) {
            .s_header.s_type = type,
            .s_header.s_local_time = get_lamport_time(),
            .s_header.s_magic = MESSAGE_MAGIC,
            .s_header.s_payload_len = 0,
            .s_payload = ""
    };
}

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
    for (local_id i = 0; i < pipe_table.size; i++) {
        free(pipe_table.data[i]);
    }
    free(pipe_table.data);
}

void print_pipes_table(local_id pid, FILE *file, struct pipe_table pipe_table) {
    fprintf(file, "\n");
    fprintf(file, "Opened file descriptors for %hhd process\n", pid);
    for (size_t i = 0; i < pipe_table.size; i++) {
        for (size_t j = 0; j < pipe_table.size; j++) {
            fprintf(file, "r: %d, w: %d | ", pipe_table.data[i][j].read_fds, pipe_table.data[i][j].write_fds);
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");
}

int open_log_files(struct log_files *log_files) {
    log_files->event_log = fopen(events_log, "w");
    log_files->pipe_log = fopen(pipes_log, "w");
    log_files->replicated_queue_log = fopen(replicated_queue_log, "w");

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

void close_extra_pipe_by_subprocess(local_id pid, struct pipe_table *pipe_table) {
    for (local_id i = 0; i < pipe_table->size; i++) {
        for (local_id j = 0; j < pipe_table->size; j++) {
            if (i != j && i != pid && j != pid) {
                close(pipe_table->data[i][j].read_fds);
                close(pipe_table->data[i][j].write_fds);
                pipe_table->data[i][j].read_fds = -1;
                pipe_table->data[i][j].write_fds = -1;
            } else if (i == pid) {
                close(pipe_table->data[i][j].read_fds);
                pipe_table->data[i][j].read_fds = -1;
            } else if (j == pid) {
                close(pipe_table->data[i][j].write_fds);
                pipe_table->data[i][j].write_fds = -1;
            }
        }
    }
}

bool * create_bool_array(local_id size){
    bool * arr = malloc(sizeof(bool) * size);
    for (local_id i = 0; i < size; i++){
        arr[i] = false;
    }
    return arr;
}

void destroy_bool_array(bool* arr){
    free(arr);
}

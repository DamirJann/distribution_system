//
// Created by damire on 14.10.2021.
//
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "auxiliary.h"
#include "banking.h"
#include "common.h"

Message create_default_message(MessageType type){
    return (Message) {
            .s_header.s_type = type,
            .s_header.s_local_time = get_physical_time(),
            .s_header.s_magic = MESSAGE_MAGIC,
            // write message to payload and its size to payload_len
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
            }
            else if (i == pid) {
                close(pipe_table->data[i][j].read_fds);
                pipe_table->data[i][j].read_fds = -1;
            } else if (j == pid) {
                close(pipe_table->data[i][j].write_fds);
                pipe_table->data[i][j].write_fds = -1;
            }
        }
    }
}

balance_t * create_balance_t_array(local_id size){
    balance_t * arr = malloc(sizeof(balance_t) * size);
    for (local_id i = 0; i < size; i++){
        arr[i] = 0;
    }
    return arr;
}

void destroy_balance_t_array(balance_t* arr){
    free(arr);
}

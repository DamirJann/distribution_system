//
// Created by damire on 14.10.2021.
//
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "auxiliary.h"


const char *EVENT_LOG_FILE_NAME = "events_log";
const char *PIPE_LOG_FILE_NAME = "pipes_log";

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

Message create_start_message() {
    return (Message) {
            .s_header.s_type = STARTED,
            .s_header.s_local_time = (int16_t) time(NULL),
            .s_header.s_magic = MESSAGE_MAGIC
    };
}

Message create_done_message() {
    return (Message) {
            .s_header.s_type = DONE,
            .s_header.s_local_time = (int16_t) time(NULL),
            .s_header.s_magic = MESSAGE_MAGIC
    };
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

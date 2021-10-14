//
// Created by damire on 14.10.2021.
//

#ifndef LAB1_AUXILIARY_H
#define LAB1_AUXILIARY_H

#endif //LAB1_AUXILIARY_H

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

void close_extra_pipe_by_subprocess(local_id, struct pipe_table*);

struct pipe_table create_pipe_table(local_id);

void destroy_pipe_table(struct pipe_table);

void print_pipes_table(local_id , FILE*, struct pipe_table);

int open_log_files(struct log_files*);

int close_log_files(struct log_files);

Message create_start_message();

Message create_done_message();

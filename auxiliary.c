//
// Created by damire on 14.10.2021.
//

struct custom_pipe {
    int read_fds;
    int write_fds;
};

struct pipe_table {
    local_id size;
    struct custom_pipe **data;
};

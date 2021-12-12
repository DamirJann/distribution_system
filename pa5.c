#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "ipc.h"
#include "utils.h"
#include "process.h"
#include "main_process.h"
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>


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

int create_processes(struct pipe_table pipe_table, struct log_files log_files, bool is_mutex_mode) {
    int parent_pid = getpid();
    for (local_id i = 1; i < pipe_table.size; i++) {
        int fork_res = fork();
        if (fork_res == 0) {
            start_process(i, parent_pid, pipe_table, log_files, is_mutex_mode);
            break;
        } else if (fork_res > 0) {
            if (i == pipe_table.size - 1) {
                start_main_process(PARENT_ID, log_files, pipe_table);
            }
        } else {
            return -1;
        }
    }
    return 0;
}

void parse_argc(int argc, char **argv, local_id *process_count, bool *is_mutex_mode) {

    static struct option long_options[] = {
            {"mutexl", no_argument, 0, 'm'},
            {"p",      no_argument, 0, 'p'},
    };
    int opt;
    while ((opt = getopt_long(argc, argv, ":mp", long_options, NULL)) != -1) {
        switch (opt) {
            case 'm' : {
                *is_mutex_mode = true;
                break;
            }
            case 'p': {
                *process_count = (local_id) strtol(argv[optind], NULL, 10);
                break;
            }
            default: {
                exit(EXIT_FAILURE);
            }
        }
    }


}

void test() {
    {
        struct replicated_queue replicated_queue = create_replicated_queue();
        assert(replicated_queue.size == 0);

        push(&replicated_queue, (struct process_request) {.lamport_time = 6, .process_id = 2});
        push(&replicated_queue, (struct process_request) {.lamport_time = 5, .process_id = 1});
        push(&replicated_queue, (struct process_request) {.lamport_time = 5, .process_id = 1});
        push(&replicated_queue, (struct process_request) {.lamport_time = 1, .process_id = 1});
        push(&replicated_queue, (struct process_request) {.lamport_time = 6, .process_id = 44});
        push(&replicated_queue, (struct process_request) {.lamport_time = 8, .process_id = 7});
        push(&replicated_queue, (struct process_request) {.lamport_time = 6, .process_id = 1});
        push(&replicated_queue, (struct process_request) {.lamport_time = 6, .process_id = 2});
        push(&replicated_queue, (struct process_request) {.lamport_time = 6, .process_id = 2});
        push(&replicated_queue, (struct process_request) {.lamport_time = 11, .process_id = 7});

        print_replicated_queue(stdout, replicated_queue);

        assert(replicated_queue.size == 10);

        pop(&replicated_queue);
        assert(replicated_queue.size == 9);

        pop(&replicated_queue);
        assert(replicated_queue.size == 8);

        push(&replicated_queue, (struct process_request) {.lamport_time = 11, .process_id = 7});

        assert(get_latest_by_pid(replicated_queue, 44).lamport_time == 6);

        print_replicated_queue(stdout, replicated_queue);

    }
}

int main(int argc, char **argv) {
    local_id process_count;
    bool is_mutex_mode;

    parse_argc(argc, argv, &process_count, &is_mutex_mode);

    struct log_files log_files;
    if (open_log_files(&log_files) != 0) {
        perror("LOG: FAILED to CREATE LOG files");
    }

    test();

    struct pipe_table pipe_table = create_pipe_table((local_id) (process_count + 1));
    if (create_pipes(&pipe_table) != 0) {
        perror("LOG: CREATION of a PIPE TABLE was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    if (create_processes(pipe_table, log_files, is_mutex_mode) != 0) {
        perror("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
        exit(EXIT_FAILURE);
    }

    return 0;

}

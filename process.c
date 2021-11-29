
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "auxiliary.h"
#include "pa2345.h"


void start_subprocess_work(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files) {
    bool is_stop_not_received = true;

    while (is_stop_not_received) {
        Message msg;
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);

        sync_lamport_time_with_another_process(msg.s_header.s_local_time);
        // event happened
        increase_lamport_counter();

        switch (msg.s_header.s_type) {
            case STOP: {
                is_stop_not_received = false;
                break;
            }
            case TRANSFER: {
                 break;
            }
            default: {
                fprintf(stderr, "%d: process %d got message with UNKNOWN type(%hd) during WORK step\n", get_lamport_time(), local_pid, msg.s_header.s_type);
                break;
            }
        }
    }
}


void start_process(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);

    // event
    increase_lamport_counter();
    Message start_msg = create_default_message(STARTED);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &start_msg);
    fprintf(log_files.event_log, log_started_fmt, get_lamport_time(), local_pid, getpid(), parent_pid);
    fprintf(stdout, log_started_fmt, get_lamport_time(), local_pid, getpid(), parent_pid);


    start_subprocess_work(local_pid, pipe_table, log_files);

    fprintf(log_files.event_log, log_process_finished, get_lamport_time(), local_pid);
    fprintf(stdout, log_process_finished, get_lamport_time(), local_pid);

}


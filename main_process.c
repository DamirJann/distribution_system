#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include "ipc.h"
#include "auxiliary.h"
#include "pa2345.h"

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

void receive_specific_message_from_all(local_id to, struct pipe_table pipe_table, MessageType msg_type) {
    local_id received_msg_count = 0;
    while (received_msg_count != pipe_table.size - 1) {
        Message msg;
        receive_any((void *) &(struct process_info) {to, pipe_table}, &msg);

        if (msg.s_header.s_type == msg_type) {
            sync_lamport_time_after_receiving(msg.s_header.s_local_time);
            received_msg_count++;
        }
    }
}

void start_main_process(local_id local_pid, struct log_files log_files, struct pipe_table pipe_table) {

    receive_specific_message_from_all(PARENT_ID, pipe_table, STARTED);
    fprintf(log_files.event_log, log_received_all_started_fmt, get_lamport_time(), local_pid);
    fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), local_pid);

    receive_specific_message_from_all(PARENT_ID, pipe_table, DONE);
    fprintf(log_files.event_log, log_received_all_done_fmt, get_lamport_time(), local_pid);
    fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), local_pid);

    Message stop_msg = create_default_message(STOP);
    send_multicast((void *) &(struct process_info) {PARENT_ID, pipe_table}, &stop_msg);

    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    wait_subprocesses(pipe_table.size - 1);
    if (close_log_files(log_files) == -1) {
        fprintf(stderr, "LOG: FAILED to CLOSE log files\n");
        exit(EXIT_FAILURE);
    }
    destroy_pipe_table(pipe_table);
    fprintf(log_files.event_log, log_process_finished, get_lamport_time(), PARENT_ID);
    fprintf(stdout, log_process_finished, get_lamport_time(), PARENT_ID);
}



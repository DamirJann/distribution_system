#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include "ipc.h"
#include "utils.h"

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
    printf("LOG: .SSFULLY WAITED all subprocesses\n");
}

void start_main_process(local_id local_pid, struct log_files log_files, struct pipe_table pipe_table) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    Message msg;
    MessageType message_count[] = {
            [STARTED] = 0,
            [DONE] = 0,
    };

    while (message_count[DONE] != pipe_table.size - 1) {
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);
        sync_lamport_time_after_receiving(msg.s_header.s_local_time);
        message_count[msg.s_header.s_type]++;
    }

    Message stop_msg = create_default_message(STOP);
    send_multicast((void *) &(struct process_info) {PARENT_ID, pipe_table}, &stop_msg);

    wait_subprocesses(pipe_table.size - 1);
    if (close_log_files(log_files) == -1) {
        fprintf(stderr, "LOG: FAILED to CLOSE log files\n");
        exit(EXIT_FAILURE);
    }
    destroy_pipe_table(pipe_table);
    fprintf(log_files.event_log, log_process_finished, get_lamport_time(), PARENT_ID);
    fprintf(stdout, log_process_finished, get_lamport_time(), PARENT_ID);
}



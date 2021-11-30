
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "auxiliary.h"
#include "pa2345.h"

void start_work_in_critical_section(){

}

void start_process_work(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files) {
    Message msg;
    struct replicated_queue replicated_queue = create_replicated_queue(pipe_table.size - 1);

    while (1){
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);
        sync_lamport_time_after_receiving(msg.s_header.s_local_time);

        switch (msg.s_header.s_type) {
            CS_REQUEST:{
                break;
            }
            CS_REPLY:{
                break;
            };
            CS_RELEASE:{
                break;
            };
        }



    }

    start_work_in_critical_section();

    destroy_replicated_queue(replicated_queue);
}


void start_process(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files,
                   bool is_mutex_mode) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);


    start_process_work(local_pid, pipe_table, log_files);


    fprintf(log_files.event_log, log_process_finished, get_lamport_time(), local_pid);
    fprintf(stdout, log_process_finished, get_lamport_time(), local_pid);
}


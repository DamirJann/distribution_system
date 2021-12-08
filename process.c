
#include <stdio.h>
#include <stdbool.h>
#include "utils.h"
#include "pa2345.h"

void start_work_in_critical_section(local_id local_pid, struct log_files log_files, int counter) {
    fprintf(stdout, "%d: process %d start work in critical section\n", get_lamport_time(), local_pid);
    fprintf(log_files.event_log, "%d: process %d start work in critical section\n", get_lamport_time(), local_pid);

    char log[256];
    sprintf(log, log_loop_operation_fmt, local_pid, counter, local_pid * 5);

    print(log);
    fprintf(log_files.event_log, log_loop_operation_fmt, local_pid, counter, local_pid * 5);

    fprintf(stdout, "%d: process %d end work in critical section\n", get_lamport_time(), local_pid);
    fprintf(log_files.event_log, "%d: process %d end work in critical section\n", get_lamport_time(), local_pid);

}

void send_requests_to_critical_section(local_id local_pid, struct pipe_table pipe_table,
                                       struct replicated_queue *replicated_queue) {
    struct process_request r = (struct process_request) {
            .lamport_time = get_lamport_time(),
            .process_id = local_pid
    };
    push(replicated_queue, r);

    sync_lamport_time_before_sending();

    Message request_msg = create_default_message(CS_REQUEST);
    write_payload(&request_msg, r);
    send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &request_msg);
}

void send_reply(local_id local_pid, struct pipe_table pipe_table, local_id to) {
    sync_lamport_time_before_sending();
    Message reply_msg = create_default_message(CS_REPLY);
    send((void *) &(struct process_info) {local_pid, pipe_table}, to, &reply_msg);
}


void handle_message(local_id local_pid, struct pipe_table pipe_table,
                    struct replicated_queue *replicated_queue, Message msg) {
    switch (msg.s_header.s_type) {
        case CS_REQUEST: {
            struct process_request request = retrieve_from_message(msg);
            push(replicated_queue, request);
            send_reply(local_pid, pipe_table, request.process_id);
            break;
        }
        case CS_REPLY: {
            break;
        }
        case CS_RELEASE: {
            pop(replicated_queue);
            break;
        }
        case STOP: {
            break;
        }
        default: {
            fprintf(stderr, "%d: process %d got message with unknown type\n", get_lamport_time(), local_pid);
        }
    }
}


void send_releases(local_id local_pid, struct pipe_table pipe_table) {
    sync_lamport_time_before_sending();
    Message release_msg = create_default_message(CS_RELEASE);
    send_multicast((void *) &(struct process_info) {local_pid, pipe_table}, &release_msg);
}

void send_done(local_id local_pid, struct pipe_table pipe_table) {
    sync_lamport_time_before_sending();
    Message release_msg = create_default_message(DONE);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &release_msg);
}

void start_listening(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                     struct replicated_queue *replicated_queue) {
    Message msg;
    int cs_request_counter = 0;
    bool is_request_send = false;
    MessageType message_count[] = {
            [CS_REPLY] = 0,
            [CS_RELEASE] = 0,
            [CS_REQUEST] = 0,
            [STOP] = 0,
    };

    while (message_count[STOP] == 0) {
        if ((cs_request_counter < local_pid * 5) && !is_request_send) {
            send_requests_to_critical_section(local_pid, pipe_table, replicated_queue);
            cs_request_counter++;
            is_request_send = true;
        }

        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);
        sync_lamport_time_after_receiving(msg.s_header.s_local_time);

        handle_message(local_pid, pipe_table, replicated_queue, msg);
        message_count[msg.s_header.s_type]++;

        if (is_in_head(*replicated_queue, local_pid) && (message_count[CS_REPLY] == pipe_table.size - 2)) {
            start_work_in_critical_section(local_pid, log_files, cs_request_counter);
            pop(replicated_queue);
            send_releases(local_pid, pipe_table);
            message_count[CS_REPLY] = 0;
            is_request_send = false;

            if (cs_request_counter == local_pid * 5) {
                send_done(local_pid, pipe_table);
            }
        }

        fprintf(log_files.replicated_queue_log, "%d: queue of %d process\n", get_lamport_time(), local_pid);
        print_replicated_queue(log_files.replicated_queue_log, *replicated_queue);
    }
}

int receive_specific_message(local_id from, struct process_info *process_info, MessageType msg_type) {
    Message msg;
    blocked_receive((void *) process_info, from, &msg);

    if (msg.s_header.s_type != msg_type) {
        return -1;
    } else {
        // event happened
        sync_lamport_time_after_receiving(msg.s_header.s_local_time);
        return 0;
    };

}


void
start_process_work_with_mutual_exclusive(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files) {
    struct replicated_queue replicated_queue = create_replicated_queue();

    start_listening(local_pid, pipe_table, log_files, &replicated_queue);

    destroy_replicated_queue(&replicated_queue);
}

void start_process_work_with_no_mutual_exclusive(local_id local_pid, struct pipe_table pipe_table,
                                                 struct log_files log_files) {
    for (int i = 1; i < local_pid * 5; i++) {
        start_work_in_critical_section(local_pid, log_files, i);
    }
    sync_lamport_time_before_sending();
    Message release_msg = create_default_message(DONE);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &release_msg);

    receive_specific_message(PARENT_ID, &(struct process_info) {local_pid, pipe_table}, STOP);

}

void start_process(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files,
                   bool is_mutex_mode) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);

    sync_lamport_time_before_sending();
    Message start_msg = create_default_message(STARTED);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &start_msg);

    if (is_mutex_mode) {
        start_process_work_with_mutual_exclusive(local_pid, pipe_table, log_files);
    } else {
        start_process_work_with_no_mutual_exclusive(local_pid, pipe_table, log_files);
    }

    fprintf(log_files.event_log, log_process_finished, get_lamport_time(), local_pid);
    fprintf(stdout, log_process_finished, get_lamport_time(), local_pid);
}


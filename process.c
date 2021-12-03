
#include <stdio.h>
#include <stdbool.h>
#include "utils.h"
#include "pa2345.h"

void start_work_in_critical_section(local_id local_pid, struct log_files log_files) {
    fprintf(stdout, "%d: process %d start work in critical section\n", get_lamport_time(), local_pid);
    fprintf(log_files.event_log, "%d: process %d start work in critical section\n", get_lamport_time(), local_pid);

    for (int i = 1; i <= local_pid * 5; i++) {
        char log[256];
        snprintf(log, 256, log_loop_operation_fmt, local_pid, i, local_pid * 5);
        print(log);
        fprintf(log_files.event_log, log_loop_operation_fmt, local_pid, i, local_pid * 5);
    }

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

void handle_cs_request(local_id local_pid, struct pipe_table pipe_table,
                       struct replicated_queue *replicated_queue, Message msg, bool is_critical_section_done,
                       bool *df) {
    struct process_request external_request = retrieve_from_message(msg);
    push(replicated_queue, external_request);

    struct process_request internal_request =  get_by_pid(*replicated_queue, local_pid);
    if (is_critical_section_done || internal_request.lamport_time > external_request.lamport_time) {
        send_reply(local_pid, pipe_table, external_request.process_id);
    } else {
        df[external_request.process_id] = true;
    }
}

void handle_message(local_id local_pid, struct pipe_table pipe_table,
                    struct replicated_queue *replicated_queue, Message msg, bool is_critical_section_done, bool *df) {
    switch (msg.s_header.s_type) {
        case CS_REQUEST: {
            handle_cs_request(local_pid, pipe_table, replicated_queue, msg, is_critical_section_done, df);
            break;
        }
        case CS_REPLY: {
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


void send_deferred_replies(local_id local_pid, struct pipe_table pipe_table, bool *df) {
    for (int i = 1; i < pipe_table.size; i++) {
        if (df[i]) {
            sync_lamport_time_before_sending();
            Message reply_msg = create_default_message(CS_REPLY);
            send((void *) &(struct process_info) {local_pid, pipe_table}, i, &reply_msg);
            df[i] = false;
        }
    }
}

void send_done(local_id local_pid, struct pipe_table pipe_table) {
    sync_lamport_time_before_sending();
    Message release_msg = create_default_message(DONE);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &release_msg);
}

void start_listening(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                     struct replicated_queue *replicated_queue, bool *df) {
    Message msg;
    bool is_critical_section_done = false;
    MessageType message_count[] = {
            [CS_REPLY] = 0,
            [CS_REQUEST] = 0,
            [STOP] = 0,
    };

    while (message_count[STOP] == 0) {
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);
        sync_lamport_time_after_receiving(msg.s_header.s_local_time);

        handle_message(local_pid, pipe_table, replicated_queue, msg, is_critical_section_done, df);
        message_count[msg.s_header.s_type]++;

        if (!is_critical_section_done && (message_count[CS_REPLY] == pipe_table.size - 2)) {
            is_critical_section_done = true;
            start_work_in_critical_section(local_pid, log_files);
            pop(replicated_queue);
            send_deferred_replies(local_pid, pipe_table, df);
            send_done(local_pid, pipe_table);
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


void start_process_work_with_mutual_exclusive(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files) {
    struct replicated_queue replicated_queue = create_replicated_queue();
    bool *df = create_bool_array(pipe_table.size);

    send_requests_to_critical_section(local_pid, pipe_table, &replicated_queue);
    start_listening(local_pid, pipe_table, log_files, &replicated_queue, df);

    destroy_bool_array(df);
    destroy_replicated_queue(&replicated_queue);
}

void start_process_work_with_no_mutual_exclusive(local_id local_pid, struct pipe_table pipe_table,
                                                 struct log_files log_files) {
    start_work_in_critical_section(local_pid, log_files);

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


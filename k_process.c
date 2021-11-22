#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <stdbool.h>
#include "ipc.h"
#include "auxiliary.h"
#include "pa2345.h"

void receive_message_from_all(local_id to, struct pipe_table pipe_table, MessageType msg_type) {
    local_id received_msg_count = 0;
    while (received_msg_count != pipe_table.size - 1) {
        Message msg = (Message) {.s_header.s_type = CS_REQUEST};
        receive_any((void *) &(struct process_info) {to, pipe_table}, &msg);
        if (msg.s_header.s_type == msg_type) {
            received_msg_count++;
        }
    }
}

void add_new_c_process_history(AllHistory *all_history, BalanceHistory balance_history) {
    all_history->s_history[all_history->s_history_len] = balance_history;
    all_history->s_history_len++;
}


void handle_balance_history(Message *msg, AllHistory *all_history) {
    BalanceHistory balance_history;
    memcpy((void *) &balance_history, (void *) msg->s_payload, msg->s_header.s_payload_len);
    add_new_c_process_history(all_history, balance_history);
}


AllHistory aggregate_history_from_c_processes(struct pipe_table pipe_table, struct log_files log_files, local_id local_pid) {
    local_id received_done_msg_count = 0;
    local_id received_balance_msg_count = 0;
    bool is_all_done_received_logged = false;

    AllHistory all_history = (AllHistory) {
            .s_history_len = 0
    };

    while (received_balance_msg_count != pipe_table.size - 1) {
        Message msg;
        receive_any((void *) &(struct process_info) {PARENT_ID, pipe_table}, &msg);

        switch (msg.s_header.s_type) {
            case DONE: {
                received_done_msg_count++;
                break;
            }
            case BALANCE_HISTORY: {
                received_balance_msg_count++;
                handle_balance_history(&msg, &all_history);
                break;
            }
            default:
                fprintf(stderr, "LOG[%d]-%d: got message with UNKNOWN type(%hd) during WORK step\n",
                        get_physical_time(), PARENT_ID, msg.s_header.s_type);
                break;
        }

        if ((is_all_done_received_logged == pipe_table.size - 1) && (is_all_done_received_logged == false)){
            is_all_done_received_logged = true;
            fprintf(log_files.event_log, log_received_all_done_fmt, get_physical_time(), local_pid);
            fprintf(stdout, log_received_all_done_fmt, get_physical_time(), local_pid);

        }


    }
    return all_history;
}


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

void start_k_process(local_id local_pid, struct log_files log_files, struct pipe_table pipe_table) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);

    receive_message_from_all(PARENT_ID, pipe_table, STARTED);
    fprintf(log_files.event_log, log_received_all_started_fmt, get_physical_time(), local_pid);
    fprintf(stdout, log_received_all_started_fmt, get_physical_time(), local_pid);

    bank_robbery((void *) &pipe_table, pipe_table.size - 1);

    receive_message_from_all(PARENT_ID, pipe_table, ACK);

    fprintf(log_files.event_log, log_received_all_ack_fmt, get_physical_time(), local_pid);
    fprintf(stdout, log_received_all_ack_fmt, get_physical_time(), local_pid);

    Message stop_msg = create_default_message(STOP);
    send_multicast((void *) &(struct process_info) {PARENT_ID, pipe_table}, &stop_msg);


    const AllHistory allHistory = aggregate_history_from_c_processes(pipe_table,log_files, local_pid);

    fprintf(log_files.event_log, log_received_all_done_fmt, get_physical_time(), local_pid);
    fprintf(stdout, log_received_all_done_fmt, get_physical_time(), local_pid);


    print_history(&allHistory);

    wait_subprocesses(pipe_table.size - 1);


    if (close_log_files(log_files) == -1) {
        fprintf(stderr, "LOG: FAILED to CLOSE log files\n");
        exit(EXIT_FAILURE);
    }
    destroy_pipe_table(pipe_table);

    fprintf(log_files.event_log, log_process_finished, get_physical_time(), PARENT_ID);
    fprintf(stdout, log_process_finished, get_physical_time(), PARENT_ID);
}

void transfer(void *parent_data, local_id src, local_id dst,
              balance_t amount) {
    struct pipe_table pipe_table = *((struct pipe_table *) parent_data);
    struct process_info process_info = (struct process_info) {PARENT_ID, pipe_table};

    TransferOrder const transfer_order = (TransferOrder) {src, dst, amount};

    Message transfer_msg = (Message) {
            .s_header.s_type = TRANSFER,
            .s_header.s_local_time = get_physical_time(),
            .s_header.s_magic = MESSAGE_MAGIC,
            .s_header.s_payload_len = sizeof(transfer_order),
    };
    memcpy((void *) transfer_msg.s_payload, (void *) &transfer_order, transfer_msg.s_header.s_payload_len);

    send((void *) &process_info, src, &transfer_msg);
}


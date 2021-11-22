
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "banking.h"
#include "auxiliary.h"
#include "pa2345.h"


void addNewBalanceState(BalanceHistory *balance_history, balance_t balance) {
    balance_history->s_history[balance_history->s_history_len] = (BalanceState) {
            .s_balance = balance,
            .s_time = get_physical_time(),
            .s_balance_pending_in = 0,
    };
    balance_history->s_history_len++;
}

void handle_transfer_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                             balance_t *init_balance, const Message received_msg) {

    TransferOrder const transfer_order;
    memcpy((void*) &transfer_order, (void*) received_msg.s_payload, received_msg.s_header.s_payload_len);

    if (transfer_order.s_src == local_pid) {
        *init_balance -= transfer_order.s_amount;

        send((void *) &(struct process_info) {local_pid, pipe_table}, transfer_order.s_dst, &received_msg);

        fprintf(log_files.event_log, log_transfer_out_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_dst );
        fprintf(stdout, log_transfer_out_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_dst );
    } else {
        *init_balance += transfer_order.s_amount;

        Message ack_msg = create_default_message(ACK);

        send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &ack_msg);
        fprintf(log_files.event_log, log_transfer_in_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_src);
        fprintf(stdout, log_transfer_in_fmt, get_physical_time(), local_pid, transfer_order.s_amount, transfer_order.s_src);
    }


}

void handle_stop_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files){
    Message done_msg = create_default_message(DONE);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &done_msg);
}

BalanceHistory start_subprocess_work(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                           balance_t init_balance) {
    BalanceHistory balanceHistory = (BalanceHistory) {.s_id = local_pid, .s_history_len = 0};

    addNewBalanceState(&balanceHistory, init_balance);

    balance_t current_balance = init_balance;
    bool is_stopped_not_received = true;

    while (is_stopped_not_received) {
        Message msg = (Message) {.s_header.s_type = 100};
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);

        switch (msg.s_header.s_type) {
            case STOP: {
                handle_stop_message(local_pid, pipe_table, log_files);
                is_stopped_not_received = false;
                break;
            }
            case TRANSFER: {
                handle_transfer_message(local_pid, pipe_table, log_files, &current_balance, msg);
                break;
            }
            default: {
                fprintf(stderr, "LOG[%d]-%d: got message with UNKNOWN type(%hd) during WORK step\n", get_physical_time(), local_pid, msg.s_header.s_type);
                break;
            }
        }

        addNewBalanceState(&balanceHistory, current_balance);
    }

    return balanceHistory;

}


void start_c_subprocess(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files,
                        balance_t init_balance) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);

    fprintf(log_files.event_log, log_started_fmt, get_physical_time(), local_pid, getpid(), parent_pid, init_balance);
    fprintf(stdout, log_started_fmt, get_physical_time(), local_pid, getpid(), parent_pid, init_balance);

    Message start_msg = create_default_message(STARTED);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &start_msg);

    BalanceHistory balanceHistory = start_subprocess_work(local_pid, pipe_table, log_files, init_balance);

    Message story_msg = create_default_message(BALANCE_HISTORY);
    story_msg.s_header.s_payload_len = sizeof(balanceHistory);
    memcpy((void *) story_msg.s_payload, (void *) &balanceHistory, story_msg.s_header.s_payload_len);

    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &story_msg);

    fprintf(log_files.event_log, log_process_finished, get_physical_time(), local_pid);
    fprintf(stdout, log_process_finished, get_physical_time(), local_pid);

}


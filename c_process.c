
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
            .s_time = balance_history->s_history_len,
            .s_balance_pending_in = 0,
    };
    balance_history->s_history_len++;

}


void fill_balance_history_before_now(BalanceHistory* balance_history){
    timestamp_t previous_timestamp = balance_history->s_history[balance_history->s_history_len-1].s_time;
    balance_t last_balance = balance_history->s_history[previous_timestamp].s_balance;

    for (int i = previous_timestamp + 1; i < get_lamport_time(); i++){
        addNewBalanceState(balance_history, last_balance);
    }
}

void handle_transfer_in_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                             BalanceHistory *balance_history, balance_t *init_balance, Message received_msg, TransferOrder transfer_order){
    // event
    increase_lamport_counter();
    received_msg.s_header.s_local_time = get_lamport_time();

    *init_balance -= transfer_order.s_amount;
    send((void *) &(struct process_info) {local_pid, pipe_table}, transfer_order.s_dst, &received_msg);

    fprintf(log_files.event_log, log_transfer_out_fmt, get_lamport_time(), local_pid, transfer_order.s_amount, transfer_order.s_dst );
    fprintf(stdout, log_transfer_out_fmt, get_lamport_time(), local_pid, transfer_order.s_amount, transfer_order.s_dst );
}

void handle_transfer_out_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                                 BalanceHistory *balance_history, balance_t *init_balance, TransferOrder transfer_order){
    // event
    increase_lamport_counter();

    *init_balance += transfer_order.s_amount;
    Message ack_msg = create_default_message(ACK);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &ack_msg);

    fprintf(log_files.event_log, log_transfer_in_fmt, get_lamport_time(), local_pid, transfer_order.s_amount, transfer_order.s_src);
    fprintf(stdout, log_transfer_in_fmt, get_lamport_time(), local_pid, transfer_order.s_amount, transfer_order.s_src);
}

void handle_transfer_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                             BalanceHistory *balance_history, balance_t *init_balance, Message received_msg) {

    TransferOrder const transfer_order = retrieve_from_message(received_msg);

    if (transfer_order.s_src == local_pid) {
        handle_transfer_in_message(local_pid, pipe_table, log_files, balance_history, init_balance, received_msg, transfer_order);
    } else {
        handle_transfer_out_message(local_pid, pipe_table, log_files, balance_history, init_balance, transfer_order);
    }
}




void handle_stop_message(const local_id local_pid, struct pipe_table pipe_table, struct log_files log_files){
    increase_lamport_counter();
    Message done_msg = create_default_message(DONE);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &done_msg);
}

void start_subprocess_work(local_id local_pid, struct pipe_table pipe_table, struct log_files log_files,
                           balance_t init_balance, BalanceHistory *balance_history) {
    balance_t current_balance = init_balance;
    bool is_stop_not_received = true;

    while (is_stop_not_received) {
        Message msg;
        receive_any((void *) &(struct process_info) {local_pid, pipe_table}, &msg);

        sync_lamport_time_with_another_process(msg.s_header.s_local_time);
        // event happened
        increase_lamport_counter();

        fill_balance_history_before_now(balance_history);

        switch (msg.s_header.s_type) {
            case STOP: {
                handle_stop_message(local_pid, pipe_table, log_files);
                is_stop_not_received = false;
                break;
            }
            case TRANSFER: {
                handle_transfer_message(local_pid, pipe_table, log_files, balance_history, &current_balance, msg);
                break;
            }
            default: {
                fprintf(stderr, "%d: process %d got message with UNKNOWN type(%hd) during WORK step\n", get_lamport_time(), local_pid, msg.s_header.s_type);
                break;
            }
        }
        addNewBalanceState(balance_history, current_balance);
    }
}


void start_c_subprocess(local_id local_pid, int parent_pid, struct pipe_table pipe_table, struct log_files log_files,
                        balance_t init_balance) {
    close_extra_pipe_by_subprocess(local_pid, &pipe_table);
    print_pipes_table(local_pid, log_files.pipe_log, pipe_table);


    BalanceHistory balance_history = (BalanceHistory) {.s_id = local_pid, .s_history_len = 0};
    addNewBalanceState(&balance_history, init_balance);

    // event
    increase_lamport_counter();
    Message start_msg = create_default_message(STARTED);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &start_msg);
    fprintf(log_files.event_log, log_started_fmt, get_lamport_time(), local_pid, getpid(), parent_pid, init_balance);
    fprintf(stdout, log_started_fmt, get_lamport_time(), local_pid, getpid(), parent_pid, init_balance);


    start_subprocess_work(local_pid, pipe_table, log_files, init_balance, &balance_history);

    // event
    increase_lamport_counter();
    Message story_msg = create_default_message(BALANCE_HISTORY);
    story_msg.s_header.s_payload_len = sizeof(balance_history);
    memcpy((void *) story_msg.s_payload, (void *) &balance_history, story_msg.s_header.s_payload_len);
    send((void *) &(struct process_info) {local_pid, pipe_table}, PARENT_ID, &story_msg);
    fprintf(log_files.event_log, log_process_finished, get_lamport_time(), local_pid);
    fprintf(stdout, log_process_finished, get_lamport_time(), local_pid);

}


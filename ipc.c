#include <stdio.h>
#include <unistd.h>
#include "ipc.h"
#include "utils.h"

int send_multicast(void *self, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    for (local_id i = 1; i < pipe_table.size; i++) {
        if (i == from) {
            continue;
        }
        send(self, i, msg);
    }
    return 0;
}

int send(void *self, local_id dst, const Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id from = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][dst].write_fds;

    if (write(fd, msg, msg->s_header.s_payload_len + sizeof(MessageHeader)) == -1) {
        fprintf(stderr, "%d: process %d FAILED to SENT msg = %s to %hhd\n", get_lamport_time(), from,
                msg->s_payload, dst);
        return -1;
    } else {
//        printf("%d: process %d SUCCESSFULLY SENT msg('%s', %s) to %hhd process\n", get_lamport_time(), from, msg->s_payload, messageTypes[msg->s_header.s_type], dst);
        return 0;
    }
}

int receive(void *self, local_id from, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    local_id to = ((struct process_info *) self)->id;
    int fd = pipe_table.data[from][to].read_fds;
    if (read(fd, &msg->s_header, sizeof(MessageHeader)) < 1) {
        return -1;
    }

    if (read(fd, msg->s_payload, msg->s_header.s_payload_len) == -1) {
        fprintf(stderr, "%d: %d FAILED to RECEIVE PAYLOAD from %hhd process\n", get_lamport_time(), to, from);
        return -1;
    }

//    printf("%d: process %d SUCCESSFULLY RECEIVED msg('%s', %s) from %hhd process\n", get_lamport_time(), to, msg->s_payload, messageTypes[msg->s_header.s_type], from);
    return 0;
}

int receive_any(void *self, Message *msg) {
    struct pipe_table pipe_table = ((struct process_info *) self)->pipe_table;
    while (1) {
        for (local_id i = 0; i < pipe_table.size; i++) {
            if (receive(self, i, msg) == 0) {
                return 0;
            }
        }
    }
}

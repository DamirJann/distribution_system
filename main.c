#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <threads.h>
thread_local int pid_counter = 0;

size_t get_align_pid(size_t max_process_count){
    return getpid() % max_process_count;
}


void start_subprocess(size_t pid){
    printf("LOG: SUBPROCESS with PID = %zu STARTED\n", pid);
}

void start_parent_process(size_t pid){
    printf("LOG: PARENT PROCESS with pid = %lu started\n", pid);
}

int create_subprocesses(size_t process_count){
    for (size_t i = 0; i < process_count; i++){
        int fork_res = fork();
        if (fork_res == 0){
            start_subprocess(get_align_pid(process_count + 1));
            break;
        } else if (fork_res > 0){
            if (i == 0) {
                start_parent_process(get_align_pid(process_count + 1));
            }
        } else {
            printf("LOG: CREATION of a CHILD PROCESS was UNSUCCESSFUL\n");
            return -1;
        }
    }
}







int main(int argc, char **argv) {
    size_t process_count = atoi(argv[2]);

    if (process_count == 0){
        printf("LOG: process count is not specified manually\n");
        printf("LOG: default process count is 2\n");
        process_count = 10;
    }

    int res = create_subprocesses(process_count);



    return 0;
}

.PHONY: all run clean


CC=gcc

all: pa5.out

run: all
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(pwd -P)"
	LD_PRELOAD="$(shell pwd -P)/libruntime.so" ./pa5.out -p 8 --mutexl
clean:
	rm pa5.out

pa5.out: pa5.c utils.c process.c main_process.c ipc.c
	$(CC) -o $@ -std=c99 -Wall -pedantic -g -O0 $^ $(CFLAGS) -L. -lruntime # -fsanitize=address

%.c:
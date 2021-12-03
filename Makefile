.PHONY: all run clean


CC=gcc

all: pa4.out

run: all
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(pwd -P)"
	LD_PRELOAD="$(shell pwd -P)/libruntime.so" ./pa4.out -p 20 --mutexl
clean:
	rm pa4.out

pa4.out: pa4.c utils.c process.c main_process.c ipc.c
	$(CC) -o $@ -std=c99 -Wall -pedantic -g -O0 $^ $(CFLAGS) -L. -lruntime # -fsanitize=address

%.c:
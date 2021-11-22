.PHONY: all clean run

CC=gcc

all: pa23.out

run: all
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:$(pwd -P)"
	LD_PRELOAD="$(pwd -P)/libruntime.so" ./pa23.out -p 3 5 6 7

clean:
	rm pa23.out

pa23.out: pa23.c bank_robbery.c auxiliary.c c_process.c k_process.c
	$(CC) -o $@ -std=c99 -Wall -pedantic -g -O0 $^ $(CFLAGS) -L. -lruntime # -fsanitize=address

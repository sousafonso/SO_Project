CC = gcc
CFLAGS = -Wall -g -Iinclude $(shell pkg-config --cflags glib-2.0)
LDFLAGS = $(shell pkg-config --libs glib-2.0) -lm -lncurses
OUTPUT_FOLDER = output
PARALLEL_TASKS = 1
SCHED_POLICY = fcfs

all: server client

server: bin/orchestrator

client: bin/client

folders:
    @mkdir -p src include obj bin $(OUTPUT_FOLDER)

bin/orchestrator: obj/orchestrator.o obj/main.o
    $(CC) $^ -o $@ $(LDFLAGS)

bin/client: obj/client.o obj/main.o
    $(CC) $^ -o $@ $(LDFLAGS)

obj/%.o: src/%.c include/%.h | folders
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -rf obj/* bin/* $(OUTPUT_FOLDER)/*


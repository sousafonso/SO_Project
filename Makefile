CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = $(shell pkg-config --libs glib-2.0) -lm -lncurses
OUTPUT_FOLDER = output

all: orchestrator client

orchestrator: bin/orchestrator

client: bin/client

folders:
	@mkdir -p src include obj bin $(OUTPUT_FOLDER)

bin/orchestrator: obj/orchestrator.o | folders
	$(CC) $^ -o $@ $(LDFLAGS)

bin/client: obj/client.o | folders
	$(CC) $^ -o $@ $(LDFLAGS)

obj/%.o: src/%.c | folders
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o bin/orchestrator bin/client

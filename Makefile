CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = $(shell pkg-config --libs glib-2.0) -lm -lncurses
OUTPUT_FOLDER = output

all: orchestrator client

bin/orchestrator: orchestrator

bin/client: client

folders:
	@mkdir -p src includes obj bin $(OUTPUT_FOLDER)

orchestrator: obj/orchestrator.o | folders
	$(CC) $^ -o $@ $(LDFLAGS)

client: obj/client.o | folders
	$(CC) $^ -o $@ $(LDFLAGS)

obj/%.o: src/%.c | folders
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	if [ -p $(FIFO_PATH) ]; then rm -f $(FIFO_PATH); fi
	rm -r obj/ orchestrator client output/ bin/ state.txt completed_tasks.txt
	


CC = gcc
CFLAGS = -Wall -g -Iincludes
LDFLAGS =

all: client orchestrator

client: 
	$(CC) $(CFLAGS) src/client.c -o client $(LDFLAGS)

orchestrator: 
	$(CC) $(CFLAGS) src/orchestrator.c -o orchestrator $(LDFLAGS)

clean:
	rm -f client orchestrator

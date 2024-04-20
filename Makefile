CC = gcc
CFLAGS = -Wall -g -Iinclude $(shell pkg-config --cflags glib-2.0)
LDFLAGS = $(shell pkg-config --libs glib-2.0) -lm -lncurses
OUTPUT_FOLDER = output
PARALLEL_TASKS = 1
SCHED_POLICY = fcfs

# Definindo o alvo padrão como "all", que compilará ambos os clientes e servidores
all: client server

# Alvo "server": compila o binário do servidor e o executa
server: bin/orchestrator
	./bin/orchestrator $(OUTPUT_FOLDER) $(PARALLEL_TASKS) $(SCHED_POLICY)

# Alvo "client": compila o binário do cliente e o executa
client: bin/client
	./bin/client -u 1000 "ls -l | grep .txt"

# Criação de pastas necessárias (se não existirem)
folders:
	@mkdir -p src include obj bin $(OUTPUT_FOLDER)

# Compilação do binário do servidor
bin/orchestrator: obj/orchestrator.o
	$(CC) $^ -o $@ $(LDFLAGS)

# Compilação do binário do cliente
bin/client: obj/client.o
	$(CC) $^ -o $@ $(LDFLAGS)

# Compilação dos arquivos de objeto
obj/%.o: src/%.c include/%.h | folders
	$(CC) $(CFLAGS) -c $< -o $@

# Alvo "clean": remove os arquivos compilados e as pastas de saída
clean:
	rm -rf obj/* bin/* $(OUTPUT_FOLDER)/*


CC=gcc
CFLAGS=-I./include
SRCDIR=src
BINDIR=bin
OBJDIR=obj

SOURCES=$(wildcard $(SRCDIR)/**/*.c) $(wildcard $(SRCDIR)/*.c)
OBJECTS=$(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

all: directories server client

directories:
    @mkdir -p $(BINDIR)
    @mkdir -p $(OBJDIR)/core
    @mkdir -p $(OBJDIR)/networking
    @mkdir -p $(OBJDIR)/logging
    @mkdir -p $(OBJDIR)/utilities
    @mkdir -p $(OBJDIR)/cli

server: $(OBJECTS)
    $(CC) $(CFLAGS) -o $(BINDIR)/server $(OBJDIR)/networking/server.o $(OBJDIR)/logging/logger.o $(OBJDIR)/utilities/utils.o $(OBJDIR)/core/task_manager.o $(OBJDIR)/server_main.o

client: $(OBJECTS)
    $(CC) $(CFLAGS) -o $(BINDIR)/client $(OBJDIR)/cli/cli.o $(OBJDIR)/networking/client.o $(OBJDIR)/utilities/utils.o $(OBJDIR)/cli_main.o

$(OBJDIR)/%.o: $(SRCDIR)/%.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -rf $(BINDIR)/*
    rm -rf $(OBJDIR)/*

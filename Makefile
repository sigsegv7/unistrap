CFILES = $(shell find . -name "*.c")
OFILES = $(CFILES:.c=.o)

CFLAGS = -Wall -pedantic
CC = gcc

.PHONY: all
all: $(OFILES)
	$(CC) $^ -o unistrap

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

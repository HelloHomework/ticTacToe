SHELL = /bin/bash
CC = gcc
CFLAGS= -pthread -O0
DEBUG_FLAGS = -g -W -Wall -D DEBUG -pthread
SRC = $(wildcard *.c)
EXE = $(patsubst %.c, %, $(SRC))

all: server.c client.c
	${CC} ${CFLAGS} server.c -o server
	${CC} ${CFLAGS} client.c -o client

debug: server.c client.c
	${CC} ${DEBUG_FLAGS} server.c -o server
	${CC} ${DEBUG_FLAGS} client.c -o client

clean:
	rm ./server ./client


ifndef port
port = 8346
endif

ifndef hostname
hostname = 127.0.0.1
endif

CC=gcc
CFLAGS= -pthread -Wall -g

all: udp-server udp-client

udp-server: udp-server.c
	$(CC) $(CFLAGS) -o $@ $^

udp-client: udp-client.c
	$(CC) $(CFLAGS) -o $@ $^

run-server: udp-server
	./udp-server $(port)

run-client: udp-client
	./udp-client $(port)
	# ./udp-client $(hostname) $(port)

.PHONY: clean

clean:
	rm -f udp-server udp-client

clean-server:
	rm -f udp-server

clean-client:
	rm -f udp-client

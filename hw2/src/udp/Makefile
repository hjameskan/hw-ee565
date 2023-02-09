ifndef port
port = 8346
endif

ifndef hostname
hostname = 127.0.0.1
endif

CC=gcc
CFLAGS= -pthread -Wall -g

all: udp-server udp-client

udp-server: udp-server.o hashmap.o
	$(CC) $(CFLAGS) -o $@ $^

udp-server.o: udp-server.c
	$(CC) $(CFLAGS) -c $<

hashmap.o: hashmap.c
	$(CC) $(CFLAGS) -c $<

udp-client: udp-client.c
	$(CC) $(CFLAGS) -o $@ $^

run-server: udp-server
	./udp-server $(port)

run-client: udp-client
	./udp-client $(port)
	# ./udp-client $(hostname) $(port)

.PHONY: clean

clean:
	rm -f udp-server udp-client hashmap.o udp-server.o

clean-server:
	rm -f udp-server hashmap.o udp-server.o

clean-client:
	rm -f udp-client
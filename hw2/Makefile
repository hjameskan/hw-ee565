ifndef port1
port1 = 8345
endif

ifndef port2
port2 = 8346
endif

ifndef port3
port3 = 8347
endif

CC=gcc
CFLAGS= -pthread -g

.PHONY: clean
all: vodserver peerserver

vodserver: src/main.o src/socket_utils.o src/http_utils.o src/uri_parse.o src/hashmap.o
	$(CC) $(CFLAGS) $^ -o $@

peerserver: src/udp-server.o src/hashmap.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run-peer: peerserver
	./peerserver $(port3)

run: vodserver
	./vodserver $(port1) $(port2)

test:
	(cd scripts; ./test_script.sh $(port1) | tee test.log)

clean:
	rm -f src/*.o vodserver peerserver
	rm -rf scripts/dump
	rm -f scripts/test.log

clean-peer:
	rm -f udp-server hashmap.o peerserver.o

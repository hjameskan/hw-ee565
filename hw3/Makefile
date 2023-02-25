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
CFLAGS= -pthread -g -I src/lib/cJSON -I src/lib/uuid4 -std=gnu99
LIBS= src/lib/cJSON/cJSON.c src/lib/uuid4/uuid4.c

.PHONY: clean
all: vodserver peerserver

vodserver: src/main.o src/socket_utils.o src/http_utils.o src/uri_parse.o src/hashmap.o src/json_utils.o src/conf_utils.o src/hmap_utils.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

peerserver: src/udp-server.o src/hashmap.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run-server: peerserver
	./peerserver $(port3)

run: vodserver
	./vodserver $(port1) $(port2)

test:
	(cd scripts; ./test_script.sh $(port1) | tee test.log)


peer-test: ./scripts/run_nodes.sh
	./scripts/run_nodes.sh



clean:
	rm -f src/*.o vodserver peerserver
	rm -rf scripts/dump
	rm -f scripts/test.log

clean-server:
	rm -f udp-server hashmap.o peerserver.o

ifndef port
port1 = 8345
port2 = 8346
endif

.PHONY: clean
all: vodserver

vodserver: src/main.c
	gcc -Wall -o vodserver src/main.c src/socket_utils.c src/http_utils.c src/uri_parse.c

run: vodserver
	./vodserver $(port1) $(port2)

test:
	(cd scripts; ./test_script.sh $(port) | tee test.log)

clean:
	rm -f vodserver
	rm -rf scripts/dump
	rm -f scripts/test.log

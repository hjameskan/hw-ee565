ifndef port
port = 8345
endif

.PHONY: clean
all: vodserver

vodserver: src/main.c
	gcc -Wall -o vodserver src/main.c

run: vodserver
	./vodserver $(port)

test:
	(cd scripts; ./test_script.sh $(port) | tee test.log)

clean:
	rm -f vodserver
	rm -rf scripts/dump
	rm -f scripts/test.log

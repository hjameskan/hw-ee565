ifndef port
port = 8345
endif

all: vodserver

vodserver: src/main.c
	gcc -Wall -o vodserver src/main.c

run: vodserver
	./vodserver $(port)

clean:
	rm -f vodserver

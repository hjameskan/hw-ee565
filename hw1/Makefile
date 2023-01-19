ifndef port
port = 8345
endif

all: vodserver

vodserver: src/main.c
	gcc -o vodserver src/main.c

run: vodserver
	./vodserver $(port)

clean:
	rm -f vodserver

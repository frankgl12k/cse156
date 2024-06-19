all: bin/myproxy

bin/myproxy: src/myproxy.c
	gcc -pthread -o bin/myproxy src/myproxy.c -lcrypto -lssl
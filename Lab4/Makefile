all: bin/myclient bin/myserver

bin/myclient: src/myclient.c
	gcc -pthread -o bin/myclient src/myclient.c

bin/myserver: src/myserver.c
	gcc -o bin/myserver src/myserver.c -lm
all: server client

clean:
	rm server client *.o

server: server.o
	gcc -Wall server.o -o server

client: client.o
	gcc -Wall client.o -o client

server.o: server.c defns.h
	gcc -O0 -g -Wall -DDEBUG -c server.c

client.o: client.c defns.h
	gcc -O0 -g -Wall -DDEBUG -c client.c
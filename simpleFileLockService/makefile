all: FT_SimpleFileLock_Server SimpleFileLock_Server SimpleFileLock_Client

clean:
	rm bin/* *.o

FT_SimpleFileLock_Server: FT_SimpleFileLock_Server.o
	g++ -Wall -L../logcabin/build FT_SimpleFileLock_Server.o -o bin/FT_SimpleFileLock_Server -llogcabin -lprotobuf -lpthread -lcryptopp

SimpleFileLock_Server: SimpleFileLock_Server.o
	gcc -Wall SimpleFileLock_Server.o -o bin/SimpleFileLock_Server

SimpleFileLock_Client: SimpleFileLock_Client.o
	gcc -Wall SimpleFileLock_Client.o -o bin/SimpleFileLock_Client

FT_SimpleFileLock_Server.o: FT_SimpleFileLock_Server.cc
	g++ -O0 -g -Wall -fpermissive -DDEBUG -I../logcabin/include/ -c FT_SimpleFileLock_Server.cc

SimpleFileLock_Server.o: SimpleFileLock_Server.c defns.h
	gcc -O0 -g -Wall -DDEBUG -c SimpleFileLock_Server.c

SimpleFileLock_Client.o: SimpleFileLock_Client.c defns.h
	gcc -O0 -g -Wall -DDEBUG -c SimpleFileLock_Client.c
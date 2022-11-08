CC=g++ -g
CFLAGS=-Wall -g -pthread
all: server

objs/logging.o: src/logging.h src/logging.cxx
	$(CC) -c src/logging.cxx -o objs/logging.o $(CFLAGS)

objs/handlers.o: src/state.h src/handlers.h src/network.h src/logging.h src/duckchat.h
	$(CC) -c src/handlers.cxx -o objs/handlers.o $(CFLAGS)

objs/network.o: src/network.cxx src/network.h src/duckchat.h src/logging.h
	$(CC) -c src/network.cxx -o objs/network.o $(CFLAGS)

objs/state.o: src/state.cxx src/state.h src/network.h src/logging.h src/duckchat.h
	$(CC) -c src/state.cxx -o objs/state.o $(CFLAGS)

server: src/server.cxx objs/state.o objs/handlers.o objs/logging.o objs/network.o
	$(CC) src/server.cxx objs/state.o objs/handlers.o objs/logging.o objs/network.o -o server $(CFLAGS)

clean:
	rm objs/*.o
	rm server
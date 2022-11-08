CC=gcc -g 
CFLAGS=-Wall -W -g -Werror -pthread -I. -Iadts
all: client server

objs/iterator.o: adts/iterator.h adts/iterator.c
	$(CC) -c -Iadts adts/iterator.c -o objs/iterator.o $(CFLAGS)

objs/hashmap.o: adts/hashmap.c adts/iterator.h adts/hashmap.h objs/iterator.o
	$(CC) -c -Iadts adts/hashmap.c  -o objs/hashmap.o $(CFLAGS)

objs/linkedlist.o: adts/linkedlist.c adts/iterator.h adts/linkedlist.h objs/iterator.o
	$(CC) -c adts/linkedlist.c -o objs/linkedlist.o $(CFLAGS)

objs/state.o: objs/hashmap.o objs/linkedlist.o state.c state.h send.h duckchat.h
	$(CC) -c state.c -o objs/state.o $(CFLAGS)

server: server.c objs/state.o server_helpers.h state.h send.h
	$(CC) server.c objs/state.o objs/hashmap.o objs/linkedlist.o objs/iterator.o -o server $(CFLAGS)

objs/raw.o: raw.c raw.h
	$(CC) -c raw.c -o objs/raw.o $(CFLAGS)

client: client.c objs/raw.o
	$(CC) client.c objs/raw.o -o client $(CFLAGS)

clean:
	rm -f client server objs/*.o


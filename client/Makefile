#CC=arm-linux-gcc
CFLAGS=-I../sys

all: test-client

test-client: main.o net.o
	$(CC) -lpthread -o $@ $^

main.o: main.c

net.o: net.c

client.o: client.c

libclient.a: client.o net.o ../sys/cmd.o
	ar -rc $@ $^

clean:
	rm -rf *.o test-client

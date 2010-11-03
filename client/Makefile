#CC=arm-linux-gcc

all: test-client

test-client: main.o net.o
	$(CC) -lpthread -o $@ $^

main.o: main.c

net.o: net.c

clean:
	rm -rf *.o test-client

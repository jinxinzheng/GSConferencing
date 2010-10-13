
all: test-client

test-client: main.o
	$(CC) -lpthread -o $@ $^

main.o: main.c

clean:
	rm -rf *.o test-client

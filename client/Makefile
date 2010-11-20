#CC:=arm-linux-gcc
CFLAGS:=-I../sys

SRCS := $(wildcard *.c)

all: test-client libclient.a

test-client: main.o net.o
	$(CC) -lpthread -o $@ $^

libclient.a: client.o net.o ../sys/cmd.o
	$(AR) -rc $@ $^

.depend: Makefile $(SRCS)
	@$(CC) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -rf *.o .depend test-client libclient.a

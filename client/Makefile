#CC:=arm-linux-gcc
CFLAGS := -I..
LDFLAGS := -lpthread

SRCS := $(wildcard *.c)

all: test-client libclient.a

test-client: main.o net.o
	$(CC) $(LDFLAGS) -o $@ $^

libclient.a: client.o net.o ../cmd/cmd.o
	$(AR) -rcv $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -rf *.o .depend test-client libclient.a

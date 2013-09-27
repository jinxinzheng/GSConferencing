CFLAGS 	= -m32 -O2 -I../include -I../client/vo-aacenc/include -I../client/opencore-aacdec/include/
LDFLAGS = -m32 -lpthread -L../client/vo-aacenc/x86 -lvo-aacenc -L../client/opencore-aacdec/x86 -laacdec

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

wan: $(OBJS) thread.o
	$(CC) -o $@ $^ $(LDFLAGS)

thread.o: ../common/thread.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -f *.o wan

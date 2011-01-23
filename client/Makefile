#CC:=arm-linux-gcc
CFLAGS := -I..
LDFLAGS := -lpthread

SRCS := $(wildcard *.c)

ifdef ARCH
libclient := libclient-$(ARCH).a
test_client := test-client-$(ARCH)
else
test_client := test-client
libclient := libclient.a
endif

all: $(test_client) $(libclient)

$(test_client): main.o net.o
	$(CC) $(LDFLAGS) -o $@ $^

$(libclient): client.o net.o
	$(AR) -rcv $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -rf *.o .depend $(test_client) apitest $(libclient)

apitest: apitest.o $(libclient)

pass: pass.o ../sha/sha128.o

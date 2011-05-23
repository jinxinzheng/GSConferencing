#CC:=arm-linux-gcc
CFLAGS := -pipe -I.. -O2 -Wall -Wno-unused-variable
LDFLAGS := -pipe -lpthread -lrt

SRCS := $(wildcard *.c)

ifdef ARCH
libclient := libclient-$(ARCH).a
test_client := test-client-$(ARCH)
else
test_client := test-client
libclient := libclient.a
endif

all: $(test_client) $(libclient) libclient.so apitest

$(test_client): main.o net.o
	$(CC) $(LDFLAGS) -o $@ $^

$(libclient): client.o net.o
	$(AR) -rcv $@ $^

libclient.so: client.o net.o
	$(CC) -shared -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -rf *.o .depend $(test_client) apitest $(libclient) libclient.so

apitest: apitest.o $(libclient)

pass: pass.o ../sha/sha128.o

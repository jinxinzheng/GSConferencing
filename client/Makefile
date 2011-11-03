#CC:=arm-linux-gcc
CFLAGS := -pipe -I.. -O2 -Wall
LDFLAGS := -pipe -lpthread -lrt

ifneq (,$(ARCH))
	CFLAGS += -march=$(ARCH)
endif

SRCS := $(wildcard *.c)

ifneq (,$(HOST))
libclient := libclient-$(HOST).a
test_client := test-client-$(HOST)
else
test_client := test-client
libclient := libclient.a
endif

all: $(test_client) $(libclient) apitest cyctld

$(test_client): main.o net.o ../common/thread.o
	$(CC) $(LDFLAGS) -o $@ $^

$(libclient): client.o net.o ../common/thread.o
	$(AR) -rcv $@ $^

libclient.so: client.o net.o
	$(CC) -shared -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

apitest: apitest.o $(libclient)

pass: pass.o ../sha/sha128.o

cyctld: cyctld.o ../common/ping.o inifile.o

clean:
	rm -rf *.o .depend $(test_client) apitest $(libclient) libclient.so cyctld

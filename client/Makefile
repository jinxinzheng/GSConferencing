#CC:=arm-linux-gcc
CFLAGS += -I..
LDFLAGS := -lpthread -lrt

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

aac_libs = vo-aacenc/lib/libvo-aacenc.a opencore-aacdec/lib/libaacdec.a

all: $(test_client) $(libclient) apitest cyctld netplay bcast ucast

$(test_client): test_client.o net.o ../common/thread.o
	$(CC) $(LDFLAGS) -o $@ $^

$(libclient): client.o net.o mix.o pcm.o adpcm.o rbudp.o aac.o ../common/thread.o ../.version.o
	$(AR) -rcv $@ $^

libclient.so: client.o net.o
	$(CC) -shared -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

apitest: apitest.o $(libclient) $(aac_libs)

pass: pass.o ../sha/sha128.o

cyctld: cyctld.o ../common/ping.o inifile.o

netplay: netplay.o $(libclient)

bcast: bcast.o $(libclient) $(aac_libs)

ucast: ucast.o $(libclient) ../sha/sha128.o

clean:
	rm -rf *.o .depend $(test_client) apitest $(libclient) libclient.so cyctld

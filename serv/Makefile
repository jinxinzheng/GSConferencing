#CC:=arm-linux-gcc
CFLAGS:=-I. -I..
LDFLAGS:=-lpthread -lsqlite

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

ifdef ARCH
sys = daya-sys-$(ARCH)
else
sys = daya-sys
endif


all: $(sys)

$(sys): main.o opts.o cast.o packet.o devctl.o tag.o network.o sys.o \
	cmd_handler.o cmd_reg.o cmd_sub.o cmd_discctrl.o cmd_votectrl.o cmd_service.o cmd_msgctrl.o cmd_videoctrl.o cmd_filectrl.o \
	db/md.o db/db.o ../sha/sha128.o
	$(CC) $(LDFLAGS) -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -f *.o .depend $(sys) db/*.o

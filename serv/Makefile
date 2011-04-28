#CC:=arm-linux-gcc
CFLAGS:=-I. -I.. -O2 -Wall
LDFLAGS:=-lpthread -lrt -lsqlite

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

ifdef ARCH
sys = daya-sys-$(ARCH)
else
sys = daya-sys
endif

SYS_OBJS=main.o opts.o recover.o cast.o packet.o devctl.o group.o tag.o network.o sys.o hbeat.o pcm.o interp.o ptc.o\
	cmd_handler.o cmd_debug.o cmd_tag.o cmd_reg.o cmd_sub.o cmd_regist.o cmd_discctrl.o cmd_interp.o cmd_votectrl.o cmd_service.o cmd_msgctrl.o cmd_videoctrl.o cmd_filectrl.o cmd_synctime.o cmd_manage.o \
	../sha/sha128.o \
	../common/debug.o \
	db/md.o db/db.o


all: $(sys)

$(sys): $(SYS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SYS_OBJS)

db/db.c: db/db.def
	make -C db

.depend: Makefile $(SRCS) db/db.c
	$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	make -C db clean
	rm -f *.o .depend $(sys) db/*.o

#.PHONY: db

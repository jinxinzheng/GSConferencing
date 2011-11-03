#CC:=arm-linux-gcc
CFLAGS := -pipe -I. -I.. -O2 -Wall
LDFLAGS := -pipe -lpthread -lrt -lsqlite

ifneq (,$(ARCH))
	CFLAGS += -march=$(ARCH)
endif

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

ifneq (,$(HOST))
sys = daya-sys-$(HOST)
else
sys = daya-sys
endif


SYS_OBJS=main.o opts.o recover.o cast.o packet.o devctl.o group.o tag.o network.o sys.o hbeat.o pcm.o interp.o ptc.o\
	async.o block.o ../threadpool/threadpool.o \
	init.o \
	enounce.o \
	cmd_handler.o \
	cmd_debug.o \
	cmd_tag.o \
	cmd_reg.o \
	cmd_sub.o \
	cmd_regist.o \
	cmd_discctrl.o \
	cmd_interp.o \
	cmd_votectrl.o \
	cmd_service.o \
	cmd_msgctrl.o \
	cmd_videoctrl.o \
	cmd_filectrl.o \
	cmd_sysconfig.o \
	cmd_manage.o \
	cmd_misc.o \
	cmd_ptc.o \
	../sha/sha128.o \
	../common/ping.o \
	../common/thread.o \
	db/md.o db/db.o

SYS_LDS = sys.lds


all: db $(sys)

$(sys): $(SYS_OBJS) $(SYS_LDS)
	$(CC) $(LDFLAGS) -o $@ $(SYS_OBJS) $(SYS_LDS)

db:
	make -C db

.PHONY: db

db/db_impl.h: db

db/mng.h: db

.depend: $(SRCS) db/db_impl.h db/mng.h
	$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	make -C db clean
	rm -f *.o .depend $(sys) db/*.o

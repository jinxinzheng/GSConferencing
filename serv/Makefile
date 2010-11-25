#CC:=arm-linux-gcc
CFLAGS:=-I..
LDFLAGS:=-lpthread

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

ifdef ARCH
sys = daya-sys-$(ARCH)
else
sys = daya-sys
endif


all: $(sys)

$(sys): main.o cast.o devctl.o network.o sys.o ../cmd/cmd.o cmd_handler.o cmd_reg.o cmd_sub.o cmd_discctrl.o cmd_votectrl.o
	$(CC) $(LDFLAGS) -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -f *.o .depend $(sys)

#CC:=arm-linux-gcc

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

all: daya-sys

daya-sys: main.o cast.o devctl.o network.o sys.o cmd.o cmd_handler.o cmd_reg.o cmd_sub.o cmd_votectrl.o cmd_convctrl.o
	$(CC) -lpthread -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) -MM $(SRCS) >$@
sinclude .depend

clean:
	rm -f *.o .depend daya-sys

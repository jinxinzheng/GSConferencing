#CC=arm-linux-gcc

all: daya-sys

daya-sys: main.o cast.o devctl.o network.o sys.o cmd.o cmd_handler.o cmd_reg.o cmd_sub.o cmd_votectrl.o
	$(CC) -lpthread -o $@ $^

cast.o: cast.c

devctl.o: devctl.c

network.o: network.c

sys.o: sys.c

cmd.o: cmd.c

cmd_handler.o: cmd_handler.c

cmd_reg.o: cmd_reg.c

cmd_sub.o: cmd_sub.c

cmd_votectrl.o: cmd_votectrl.c

main.o: main.c

clean:
	rm -f *.o daya-sys

#CC=arm-linux-gcc

all: daya-sys

daya-sys: main.o cast.o devctl.o network.o sys.o cmd.o
	$(CC) -lpthread -o $@ $^

cast.o: cast.c

devctl.o: devctl.c

network.o: network.c

sys.o: sys.c

cmd.o: cmd.c

main.o: main.c

clean:
	rm -f *.o daya-sys

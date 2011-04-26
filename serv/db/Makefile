CFLAGS = -I.. -I../.. -O2

all: db.o md.o

db.h: db.c

db.c: db.def
	./gen-db.sh < $<

md.o: md.c md.h db.h

clean:
	rm -f *.o db.c db.h

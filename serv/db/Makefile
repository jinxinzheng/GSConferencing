CFLAGS = -pipe -I.. -I../.. -O2

all: db.o md.o mng.h

db.h: db.c

db.c: db.def
	./gen-db.sh < $<

md.o: md.c md.h db.h

mng.h: db.def
	./gen-mng.sh < $< > $@

clean:
	rm -f *.o db.c db.h mng.h

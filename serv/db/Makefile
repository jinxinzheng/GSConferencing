CFLAGS = -pipe -I.. -I../.. -O2

all: db.o md.o mng.h

db.c: db_impl.c

db_impl.c: db.def
	./gen-db.sh < $<

md.o: md.c md.h db.h

mng.h: db.def
	./gen-mng.sh < $< > $@

clean:
	rm -f *.o db_impl.c db_impl.h mng.h

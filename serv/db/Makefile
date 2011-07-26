
all: db_impl.h mng.h

db_impl.h: db.def
	./gen-db.sh < $<

mng.h: db.def
	./gen-mng.sh < $< > $@

clean:
	rm -f db_impl.c db_impl.h mng.h

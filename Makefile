ARCH=arm-linux

ifdef ARCH
CC:=$(ARCH)-gcc
AR:=$(ARCH)-ar
export ARCH
export CC
export AR
endif

SUBDIRS:=serv client

all: $(SUBDIRS)

clean:
	rm -rf common/*.o sha/*.o threadpool/*.o
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

serv:
	$(MAKE) -C $@

client:
	$(MAKE) -C $@

tar:
	git archive -o daya.tar --prefix daya/ HEAD

.PHONY: $(SUBDIRS)

#ARCH=arm-linux

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
	rm -rf sha/*.o
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

serv:
	$(MAKE) -C $@

client:
	$(MAKE) -C $@

.PHONY: $(SUBDIRS)

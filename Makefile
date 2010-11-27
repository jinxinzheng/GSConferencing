#ARCH=arm-linux

ifdef ARCH
CC:=$(ARCH)-gcc
AR:=$(ARCH)-ar
export ARCH
export CC
export AR
endif

SUBDIRS:=serv client

all:
	for d in $(SUBDIRS); do $(MAKE) -C $$d; done

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

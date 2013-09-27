HOST=arm-linux
SERVER_ARCH=armv6
CLIENT_ARCH=armv4t

ifdef HOST
CC:=$(HOST)-gcc
AR:=$(HOST)-ar
export HOST
export CC
export AR
endif

CFLAGS = -pipe -Wall -Wextra -Wno-unused-parameter -O
export CFLAGS

SUBDIRS:=serv client

all: $(SUBDIRS)

clean:
	rm -rf *.o .*.o common/*.o sha/*.o threadpool/*.o
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

COMMON_SRC = $(wildcard common/*.c) $(wildcard sha/*.c) .version.c
COMMON_OBJ = $(COMMON_SRC:.c=.o)
common: $(COMMON_OBJ)

serv: common
	$(MAKE) -C $@ ARCH=$(SERVER_ARCH)

client: common
	$(MAKE) -C $@ ARCH=$(CLIENT_ARCH)

wan:
	$(MAKE) -C $@

tar:
	git archive -o daya.tar --prefix daya/ HEAD

.version.c:
	./gen_version.sh

.PHONY: .version.c $(SUBDIRS) wan

#CC:=arm-linux-gcc
#AR:=arm-linux-ar
#export CC
#export AR

SUBDIRS:=cmd serv client

all:
	for d in $(SUBDIRS); do $(MAKE) -C $$d; done

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

# gnome-gps make file. For GNU make.

# Clear the list of suffixes
.SUFFIXES:
# Now specify our own list of suffixes.
.SUFFIXES: .c .o .do

# Specify libraries, header files, etc.
INPUTS = `pkg-config --cflags --libs gtk+-2.0 libgps`
DEBUGFLAGS = -ggdb -Wall -c
DEBUGOBJS = gnome-gps.do icon.image.do
PRODUCTFLAGS = -Wall -c
PRODUCTOBJS = gnome-gps.o icon.image.o

SHARE=/usr/share/
EXECDIR=/usr/local/bin/

production: $(PRODUCTOBJS) gnome-gps.h icon.image.h
	gcc -Wall -O2 gnome-gps.o icon.image.o -o gnome-gps $(INPUTS)
	strip gnome-gps

debug: $(DEBUGOBJS) gnome-gps.h icon.image.h
	gcc -Wall gnome-gps.do icon.image.do -o gnome-gps $(INPUTS)


gnome-gps.o: gnome-gps.c gnome-gps.h icon.image.h
	gcc -O2 -o gnome-gps.o $(PRODUCTFLAGS) gnome-gps.c $(INPUTS)

icon.image.o: icon.image.c
	gcc -O2 -o icon.image.o $(PRODUCTFLAGS) icon.image.c $(INPUTS)

gnome-gps.do: gnome-gps.c gnome-gps.h icon.image.h
	gcc -O2 -o gnome-gps.do $(DEBUGFLAGS) gnome-gps.c $(INPUTS)

icon.image.do: icon.image.c
	gcc -O2 -o icon.image.do $(DEBUGFLAGS) icon.image.c $(INPUTS)

clean:
	rm gnome-gps *.o *.do

archive:
	git archive --format=tar HEAD | bzip2 > ../../gnome-gps.`date +%Y.%m.%d`.tar.bz2
	cd ../../ && md5sum gnome-gps.`date +%Y.%m.%d`.tar.bz2  > gnome-gps.`date +%Y.%m.%d`.md5sum
	cd ../../ && sha1sum gnome-gps.`date +%Y.%m.%d`.tar.bz2 > gnome-gps.`date +%Y.%m.%d`.sha1sum

install:
	mkdir -p $(SHARE)gnome-gps
	cp gnome-gps.logo.png $(SHARE)gnome-gps
	cp gnome-gps $(EXECDIR)
	cp gnome-gps.desktop $(SHARE)applications

uninstall:
	rm -r $(SHARE)gnome-gps
	rm $(EXECDIR)gnome-gps
	rm $(SHARE)applications/gnome-gps.desktop

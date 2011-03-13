# gnome-gps make file. For GNU make.

# Clear the list of suffixes
.SUFFIXES:
# Now specify our own list of suffixes.
.SUFFIXES: .c .o .do

# Specify libraries, header files, etc.
INPUTS = `pkg-config --cflags --libs gtk+-2.0 libgps`
DEBUGFLAGS = -ggdb -Wall
DEBUGOBJS = gnome-gps.do icon.image.do
PRODUCTFLAGS = -Wall
PRODUCTOBJS = gnome-gps.o icon.image.o

# Our very own compile rules
.c.o:
	gcc -O2 -o $@ $(PRODUCTFLAGS) -c $< $(INPUTS)
.c.do:
	gcc -o $@ $(DEBUGFLAGS) -c $< $(INPUTS)

production: $(PRODUCTOBJS)
	gcc -Wall -O2 gnome-gps.o icon.image.o -o gnome-gps $(INPUTS)
	strip gnome-gps

debug: $(DEBUGOBJS)
	gcc -Wall gnome-gps.do icon.image.do -o gnome-gps $(INPUTS)

gnome-gps.o:

icon.image.o:

gnome-gps.do:

icon.image.do:

clean:
	rm gnome-gps *.o *.do

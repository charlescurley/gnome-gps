# gnome-gps make file. For GNU make.

# Specify libraries, header files, etc.
INPUTS = `pkg-config --cflags --libs gtk+-2.0 libgps`

# Our very own compile rule
%.o : %.c
	gcc -Wall -c $< $(INPUTS)

gnome-gps: gnome-gps.o icon.image.o
	gcc -Wall gnome-gps.o icon.image.o -o gnome-gps $(INPUTS)

gnome-gps.o:

icon.image.o:

clean:
	rm gnome-gps *.o

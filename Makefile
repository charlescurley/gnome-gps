# gnome-gps make file. For GNU make.

.PHONY: gnome-gps
gnome-gps:
	cd src && make gnome-gps

.PHONY: debug
debug:
	cd src && make debug

.PHONY: validate
validate:
	cd src && make validate

.PHONY: clean
clean:
	cd src && make clean

.PHONY: install
install:
	cd src && make install

.PHONY: uninstall
uninstall:
	cd src && make uninstall

archive:
	git archive --format=tar HEAD | bzip2 > ../gnome-gps.`date +%Y.%m.%d`.tar.bz2
	cd ../ && md5sum gnome-gps.`date +%Y.%m.%d`.tar.bz2  > gnome-gps.`date +%Y.%m.%d`.md5sum
	cd ../ && sha1sum gnome-gps.`date +%Y.%m.%d`.tar.bz2 > gnome-gps.`date +%Y.%m.%d`.sha1sum

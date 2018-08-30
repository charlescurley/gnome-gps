# gnome-gps make file. For GNU make.

.PHONY: production
production:
	cd src && make production

.PHONY: debug
debug:
	cd src && make debug

.PHONY: astyle
astyle:
	cd src && make astyle

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

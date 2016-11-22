# gnome-gps make file. For GNU make.


archive:
	git archive --format=tar HEAD | bzip2 > ../gnome-gps.`date +%Y.%m.%d`.tar.bz2
	cd ../ && md5sum gnome-gps.`date +%Y.%m.%d`.tar.bz2  > gnome-gps.`date +%Y.%m.%d`.md5sum
	cd ../ && sha1sum gnome-gps.`date +%Y.%m.%d`.tar.bz2 > gnome-gps.`date +%Y.%m.%d`.sha1sum

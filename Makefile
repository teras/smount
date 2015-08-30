all:smount cdfs

smount:
	./configure

cdfs:
	cd cdfs ; make

install:uninstall
	cp smount /usr/bin/smount
	ln -s /usr/bin/smount /usr/bin/sumount
	cd cdfs ; make install

uninstall:
	rm -f /usr/bin/smount /usr/bin/sumount
	if [ -e /mnt/sessions ] ; then rm -r -f -i /mnt/sessions ; fi

clean:
	rm -f smount
	cd cdfs ; make clean

distclean:clean
	rm -f Makefile
	cd cdfs ; rm -f Makefile config.log config.status config.cache


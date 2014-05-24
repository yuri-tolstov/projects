modules modules_install help:
	make -C /lib/modules/`uname -r`/build/ M=${CURDIR} EXTRA_CFLAGS=${EXTRA_CFLAGS} $@

clean:
	make -C /lib/modules/`uname -r`/build/ M=${CURDIR} clean


#
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.4 1997/04/16 22:27:19 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd floppy.fs

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

floppy:
	cd ${.CURDIR}/../distrib/i386/floppies/ramdisk && \
	    make cleandir && make && make install


.endif	# DESTDIR check

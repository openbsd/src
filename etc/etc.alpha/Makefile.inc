#
#	etc.alpha/Makefile.inc -- alpha-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.5 1997/05/14 09:55:32 graichen Exp $

.ifdef DESTDIR

snap_md: binutils bsd boot floppy

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/alpha/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/alpha/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/alpha/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

boot:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot/boot

floppy:
	cd ${.CURDIR}/../distrib/alpha/ramdisk && \
	    make cleandir && make && make install

.endif	# DESTDIR check

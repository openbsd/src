#
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.2 1997/05/03 07:48:38 deraadt Exp $

.ifdef DESTDIR
snap_md: binutils bsd

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

.endif	# DESTDIR check

#
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.3 1997/05/13 19:46:10 deraadt Exp $

.ifdef DESTDIR
snap_md: binutils bsd bootblocks

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.4 1997/09/21 11:50:36 deraadt Exp $
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets

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

#	$OpenBSD: Makefile.inc,v 1.5 1998/04/06 20:17:19 pefo Exp $
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: binutils bsd distrib

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check

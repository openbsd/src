#	$OpenBSD: Makefile.inc,v 1.1 1997/04/09 09:04:46 pefo Exp $
#
#	etc.arc/Makefile.inc -- arc-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/arc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/arc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/arc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/arc/compile/GENERIC/bsd.ecoff \
	    ${DESTDIR}/snapshot/bsd.ecoff

.endif

#	$OpenBSD: Makefile.inc,v 1.2 1997/04/09 09:10:06 pefo Exp $
#
#	etc.wgrisc/Makefile.inc -- wgrisc-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/wgrisc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/wgrisc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/wgrisc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/wgrisc/compile/GENERIC/bsd.ecoff \
	    ${DESTDIR}/snapshot/bsd.ecoff

.endif

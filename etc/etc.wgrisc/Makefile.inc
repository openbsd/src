#	$OpenBSD: Makefile.inc,v 1.3 1997/04/09 09:11:21 pefo Exp $
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

.endif

#	$OpenBSD: Makefile.inc,v 1.3 1997/05/18 13:42:37 pefo Exp $
#
#	etc.arc/Makefile.inc -- arc-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd ramdisk

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

ramdisk:
	cd ${.CURDIR}/../distrib/arc/ramdisk && make cleandir && \
	    make && make install

.endif

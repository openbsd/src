#	$OpenBSD: Makefile.inc,v 1.9 1997/09/21 11:50:13 deraadt Exp $
#	etc.alpha/Makefile.inc -- alpha-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd distrib

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/alpha/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/alpha/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/alpha/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && \
	    make cleandir && make && make install

.endif	# DESTDIR check

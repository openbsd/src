#
#	etc.pmax/Makefile.inc -- pmax-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.4 1997/05/10 22:38:20 deraadt Exp $

.ifdef DESTDIR

snap_md: binutils bsd simpleroot
	cp ${.CURDIR}/../sys/arch/sparc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

simpleroot:
	cd ${.CURDIR}/../distrib/simpleroot && make cleandir && \
	    make && make install

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.11 2001/01/23 09:04:11 art Exp $
#	etc.alpha/Makefile.inc -- alpha-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd distrib boot

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

boot:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/netboot ${DESTDIR}/snapshot

.endif	# DESTDIR check

#
#	etc.pmax/Makefile.inc -- pmax-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.5 1997/05/13 14:30:37 graichen Exp $

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

bsd.nfs:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC_NFS
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC_NFS && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC_NFS/bsd \
	    ${DESTDIR}/snapshot/bsd.nfs

bsd.rz0:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC_rz0
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC_rz0 && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC_rz0/bsd \
	    ${DESTDIR}/snapshot/bsd.rz0

simpleroot:
	cd ${.CURDIR}/../distrib/simpleroot && make cleandir && \
	    make && make install

.endif	# DESTDIR check

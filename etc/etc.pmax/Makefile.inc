#	$OpenBSD: Makefile.inc,v 1.16 1998/04/08 18:39:30 maja Exp $
#	etc.pmax/Makefile.inc -- pmax-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd bsd.nfs bsd.rz0 distrib

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC/bsd \
	    ${DESTDIR}/bsd

bsd.nfs:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC.NFS
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.NFS && \
	    make clean && make depend && make
	elf2ecoff ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.NFS/bsd.elf \
	    ${DESTDIR}/snapshot/bsd.nfs
	elf2ecoff ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.NFS/bsd.elf \
	    ${DESTDIR}/bsd.nfs

bsd.rz0:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC.rz0
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.rz0 && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.rz0/bsd \
	    ${DESTDIR}/snapshot/bsd.rz0
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.rz0/bsd \
	    ${DESTDIR}/bsd.rz0

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check

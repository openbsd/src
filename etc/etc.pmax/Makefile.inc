#	$OpenBSD: Makefile.inc,v 1.13 1998/03/18 10:45:05 graichen Exp $
#	etc.pmax/Makefile.inc -- pmax-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd bsd.nfs bsd.rz0 bootblocks distrib

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
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC.NFS
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.NFS && \
	    make clean && make depend && make
	elf2ecoff ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.NFS/bsd.elf \
	    ${DESTDIR}/snapshot/bsd.nfs

bsd.rz0:
	cd ${.CURDIR}/../sys/arch/pmax/conf && config GENERIC.rz0
	cd ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.rz0 && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/pmax/compile/GENERIC.rz0/bsd \
	    ${DESTDIR}/snapshot/bsd.rz0

# XXX this is an really ugly hack until the bootblock source will
# compile again - or even better: someone writes it new
bootblocks:
	cp /usr/mdec/rzboot /usr/mdec/bootrz ${DESTDIR}/usr/mdec

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check

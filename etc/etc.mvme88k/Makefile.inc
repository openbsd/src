#	$OpenBSD: Makefile.inc,v 1.3 1999/03/03 05:47:08 smurph Exp $
#	etc.mvme88k/Makefile.inc -- mvme88k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bootblocks
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 bsd
bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/
distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check

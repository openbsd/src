#
#	etc.mvme88k/Makefile.inc -- mvme88k-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.1 1997/03/25 17:06:36 rahnds Exp $

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
.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.5 2003/08/09 21:18:52 miod Exp $
#	etc.mvme88k/Makefile.inc -- mvme88k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bootblocks distrib
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz

bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}
bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

.PHONY: bsd-generic bootblocks

.endif	# DESTDIR check

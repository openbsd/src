#	$OpenBSD: Makefile.inc,v 1.4 2002/05/13 07:22:43 espie Exp $
#	etc.mvme88k/Makefile.inc -- mvme88k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bootblocks
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz

bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE} && gzip -9 bsd
bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

.PHONY: bsd-generic bootblocks

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.5 2004/06/25 11:03:36 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bsd.mp distrib bootblocks
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC.MP/bsd \
	    ${DESTDIR}/snapshot/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

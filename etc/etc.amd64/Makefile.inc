#	$OpenBSD: Makefile.inc,v 1.7 2006/07/27 02:53:55 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bsd.mp bootblocks distrib
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
	cp ${DESTDIR}/usr/mdec/cdboot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/cdbr ${DESTDIR}/snapshot

.PHONY: bsd bsd.mp bootblocks

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.4 2004/03/21 23:25:11 tom Exp $

.ifdef DESTDIR
snap_md: bsd distrib bootblocks
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

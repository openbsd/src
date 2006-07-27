#	$OpenBSD: Makefile.inc,v 1.3 2006/07/27 01:58:21 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/sparc64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sparc64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sparc64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

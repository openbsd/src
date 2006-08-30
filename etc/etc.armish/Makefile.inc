#	$OpenBSD: Makefile.inc,v 1.5 2006/08/30 21:40:05 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/armish/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/armish/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/armish/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

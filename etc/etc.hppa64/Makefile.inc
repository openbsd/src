#	$OpenBSD: Makefile.inc,v 1.2 2006/07/27 02:53:55 deraadt Exp $

.ifdef DESTDIR
snap_md: bootblocks bsd distrib
	cp ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/snapshot

.PHONY: bootblocks bsd

.endif	# DESTDIR check

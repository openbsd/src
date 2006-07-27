#	$OpenBSD: Makefile.inc,v 1.15 2006/07/27 03:11:23 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/alpha/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/alpha/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/alpha/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/netboot ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

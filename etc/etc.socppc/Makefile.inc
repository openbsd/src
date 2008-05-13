#	$OpenBSD: Makefile.inc,v 1.1 2008/05/13 19:39:58 kettenis Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/socppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/socppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/socppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot/boot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

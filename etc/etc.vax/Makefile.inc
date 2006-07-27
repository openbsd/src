#	$OpenBSD: Makefile.inc,v 1.6 2006/07/27 01:58:21 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/vax/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/vax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/vax/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot/boot.mop

.PHONY: bsd bootblocks

.endif  # DESTDIR check

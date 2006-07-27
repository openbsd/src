#	$OpenBSD: Makefile.inc,v 1.4 2006/07/27 02:51:15 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvmeppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot/bootxx
	cp ${DESTDIR}/usr/mdec/bootsd ${DESTDIR}/snapshot/bootsd
	cp ${DESTDIR}/usr/mdec/installboot ${DESTDIR}/snapshot/installboot

.PHONY: bsd bootblocks

.endif	# DESTDIR check

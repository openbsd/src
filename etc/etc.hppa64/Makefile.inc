#	$OpenBSD: Makefile.inc,v 1.1 2005/04/01 16:02:16 deraadt Exp $

.ifdef DESTDIR
snap_md: boot bsd distrib
	cp ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

boot:
	cp ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/snapshot

.PHONY: bsd

.endif	# DESTDIR check

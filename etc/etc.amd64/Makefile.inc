#	$OpenBSD: Makefile.inc,v 1.1 2004/01/28 02:11:53 mickey Exp $

.ifdef DESTDIR
snap_md: boot bsd distrib
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

boot:
	cp ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/snapshot

.PHONY: bsd

.endif	# DESTDIR check

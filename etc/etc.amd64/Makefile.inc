#	$OpenBSD: Makefile.inc,v 1.2 2004/02/07 20:30:59 deraadt Exp $

.ifdef DESTDIR
snap_md: boot bsd distrib
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

boot:
	cp ${DESTDIR}/usr/mdec/biosboot ${DESTDIR}/snapshot

.PHONY: bsd

.endif	# DESTDIR check

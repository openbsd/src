#	$OpenBSD: Makefile.inc,v 1.3 2004/03/20 20:27:01 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd distrib
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

.PHONY: bsd

.endif	# DESTDIR check

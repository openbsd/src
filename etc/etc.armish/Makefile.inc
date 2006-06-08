#	$OpenBSD: Makefile.inc,v 1.3 2006/06/08 02:29:03 drahn Exp $
#	etc.armish/Makefile.inc -- armish-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib
	cp ${.CURDIR}/../sys/arch/armish/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/armish/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/armish/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

.PHONY: bsd

.endif	# DESTDIR check

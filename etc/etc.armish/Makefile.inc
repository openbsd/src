#	$OpenBSD: Makefile.inc,v 1.1 2006/06/06 00:01:49 drahn Exp $
#	etc.armish/Makefile.inc -- armish-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd notes distrib
	cp ${.CURDIR}/../sys/arch/armish/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/armish/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/armish/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${DESTDIR}/snapshot


.PHONY: bsd notes

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.11 2002/05/13 07:22:42 espie Exp $
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd notes distrib
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${DESTDIR}/snapshot

.PHONY: bsd notes

.endif	# DESTDIR check

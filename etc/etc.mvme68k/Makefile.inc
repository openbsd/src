#	$OpenBSD: Makefile.inc,v 1.8 2002/05/13 07:22:43 espie Exp $
#	etc.mvme68k/Makefile.inc -- mvme68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bootblocks distrib
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

.PHONY: bsd-generic bootblocks

.endif	# DESTDIR check

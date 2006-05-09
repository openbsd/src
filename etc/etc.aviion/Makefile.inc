#	$OpenBSD: Makefile.inc,v 1.1 2006/05/09 21:17:13 miod Exp $
#	etc.aviion/Makefile.inc -- aviion-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bootblocks distrib
	cp ${.CURDIR}/../sys/arch/aviion/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-generic:
	cd ${.CURDIR}/../sys/arch/aviion/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/aviion/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

.PHONY: bsd-generic bootblocks

.endif	# DESTDIR check

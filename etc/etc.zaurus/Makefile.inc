#	$OpenBSD: Makefile.inc,v 1.3 2005/01/10 00:28:15 deraadt Exp $
#	etc.zaurus/Makefile.inc -- zaurus-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd notes bootblocks distrib
	cp ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/zaurus/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${DESTDIR}/snapshot

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${DESTDIR}/snapshot

.PHONY: bsd notes bootblocks

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.14 2004/08/26 03:34:16 deraadt Exp $
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bsd.mp notes bootblocks distrib 
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC.MP/bsd \
	    ${DESTDIR}/snapshot/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${DESTDIR}/snapshot

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/cdboot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/cdbr ${DESTDIR}/snapshot

.PHONY: bsd notes bootblocks

.endif	# DESTDIR check

#	$OpenBSD: Makefile.inc,v 1.18 2009/04/21 09:12:26 deraadt Exp $

kernels: bsd bsd.mp bootblocks linux-notes
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

linux-notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${RELEASEDIR}

MDEXT=	bsd bsd.mp bsd.rd cd${OSrev}.iso cdemu${OSrev}.iso \
	floppy${OSrev}.fs floppyB${OSrev}.fs floppyC${OSrev}.fs \
	pxeboot cdboot cdbr INSTALL.linux

.PHONY: bsd bsd.mp bootblocks

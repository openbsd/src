#	$OpenBSD: Makefile.inc,v 1.19 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bsd.mp bootblocks linux-notes
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC.MP && \
	    ${MAKE} clean && exec ${MAKE}

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

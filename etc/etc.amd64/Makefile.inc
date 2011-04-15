#	$OpenBSD: Makefile.inc,v 1.12 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bsd.mp bootblocks
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC.MP && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

MDEXT=	bsd bsd.mp bsd.rd cd${OSrev}.iso \
	floppy${OSrev}.fs pxeboot cdboot cdbr

.PHONY: bsd bsd.mp bootblocks

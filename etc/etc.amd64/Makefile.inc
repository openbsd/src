#	$OpenBSD: Makefile.inc,v 1.9 2009/04/21 09:12:26 deraadt Exp $

kernels: bsd bsd.mp bootblocks
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/amd64/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/amd64/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/amd64/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

MDEXT=	bsd bsd.mp bsd.rd cd${OSrev}.iso floppy${OSrev}.fs \
	pxeboot cdboot cdbr

.PHONY: bsd bsd.mp bootblocks

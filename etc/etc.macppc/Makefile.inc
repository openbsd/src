#	$OpenBSD: Makefile.inc,v 1.11 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd bsd.mp bootblocks
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/ofwboot ${RELEASEDIR}/ofwboot
	cp ${DESTDIR}/usr/mdec/bsd.tbxi ${RELEASEDIR}/bsd.tbxi
	cp ${DESTDIR}/usr/mdec/boot.mac ${RELEASEDIR}/boot.mac

MDEXT=	bsd.mp cd${OSrev}.iso \
	ofwboot bsd.tbxi boot.mac

.PHONY: bsd bsd.mp bootblocks


#	$OpenBSD: Makefile.inc,v 1.11 2011/04/15 03:11:38 deraadt Exp $

kernels: bootblocks bsd bsd.mp
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC.MP && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd bsd.mp bsd.rd cd${OSrev}.iso lif${OSrev}.fs \
	xxboot

.PHONY: bsd bsd.mp bootblocks

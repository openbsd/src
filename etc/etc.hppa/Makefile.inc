#	$OpenBSD: Makefile.inc,v 1.9 2010/12/30 19:22:26 deraadt Exp $

kernels: bootblocks bsd bsd.mp
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd bsd.mp bsd.rd cd${OSrev}.iso install${OSrev}.iso lif${OSrev}.fs \
	xxboot

.PHONY: bsd bsd.mp bootblocks

#	$OpenBSD: Makefile.inc,v 1.5 2010/08/17 19:17:14 deraadt Exp $

kernels: bootblocks bsd
	cp ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd bsd.rd cd${OSrev}.iso install${OSrev}.iso lif${OSrev}.fs \
	xxboot

.PHONY: bootblocks bsd

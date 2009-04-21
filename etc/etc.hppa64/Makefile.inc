#	$OpenBSD: Makefile.inc,v 1.4 2009/04/21 09:12:26 deraadt Exp $

kernels: bootblocks bsd
	cp ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd bsd.rd xxboot

.PHONY: bootblocks bsd

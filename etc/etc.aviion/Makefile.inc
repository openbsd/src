#	$OpenBSD: Makefile.inc,v 1.3 2009/04/17 03:58:53 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/aviion/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/aviion/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/aviion/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	

.PHONY: bsd bootblocks

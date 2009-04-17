#	$OpenBSD: Makefile.inc,v 1.6 2009/04/17 03:58:53 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/armish/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/armish/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/armish/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	

.PHONY: bsd bootblocks

#	$OpenBSD: Makefile.inc,v 1.7 2009/04/21 09:12:26 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/armish/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/armish/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/armish/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd bsd.rd boot

.PHONY: bsd bootblocks

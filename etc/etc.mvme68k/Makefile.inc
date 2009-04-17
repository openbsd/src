#	$OpenBSD: Makefile.inc,v 1.10 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	

.PHONY: bsd bootblocks

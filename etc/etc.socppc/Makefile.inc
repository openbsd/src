#	$OpenBSD: Makefile.inc,v 1.3 2009/04/21 09:12:27 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/socppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/socppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/socppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot

MDEXT=	bsd bsd.rd boot

.PHONY: bsd bootblocks

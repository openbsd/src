#	$OpenBSD: Makefile.inc,v 1.2 2010/02/17 18:09:49 miod Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/loongson/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
bsd:
	cd ${.CURDIR}/../sys/arch/loongson/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/loongson/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd bsd.rd

.PHONY: bsd bootblocks

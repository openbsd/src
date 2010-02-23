#	$OpenBSD: Makefile.inc,v 1.3 2010/02/23 11:01:06 otto Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/loongson/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
bsd:
	cd ${.CURDIR}/../sys/arch/loongson/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/loongson/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd bsd.rd boot miniroot${OSrev}.fs

.PHONY: bsd bootblocks

#	$OpenBSD: Makefile.inc,v 1.4 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/loongson/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
bsd:
	cd ${.CURDIR}/../sys/arch/loongson/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/loongson/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd bsd.rd boot miniroot${OSrev}.fs

.PHONY: bsd bootblocks

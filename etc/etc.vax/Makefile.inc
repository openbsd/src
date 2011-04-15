#	$OpenBSD: Makefile.inc,v 1.11 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/vax/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/vax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/vax/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot.mop

MDEXT=	bsd bsd.rd boot.mop cd${OSrev}.iso floppy${OSrev}.fs

.PHONY: bsd bootblocks

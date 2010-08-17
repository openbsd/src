#	$OpenBSD: Makefile.inc,v 1.9 2010/08/17 19:17:14 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/vax/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/vax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/vax/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot.mop

MDEXT=	bsd bsd.rd boot.mop cd${OSrev}.iso install${OSrev}.iso floppy${OSrev}.fs

.PHONY: bsd bootblocks

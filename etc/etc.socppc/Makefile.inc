#	$OpenBSD: Makefile.inc,v 1.5 2010/08/17 19:17:14 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/socppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/socppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/socppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot
	cp ${DESTDIR}/usr/mdec/boot.elf ${RELEASEDIR}/boot.elf

MDEXT=	bsd bsd.bin bsd.rd boot boot.elf miniroot${OSrev}.fs

.PHONY: bsd bootblocks

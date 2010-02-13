#	$OpenBSD: Makefile.inc,v 1.4 2010/02/13 19:39:56 kettenis Exp $

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

MDEXT=	bsd bsd.rd boot boot.elf

.PHONY: bsd bootblocks

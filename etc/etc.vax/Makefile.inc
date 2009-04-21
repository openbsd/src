#	$OpenBSD: Makefile.inc,v 1.8 2009/04/21 09:12:29 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/vax/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/vax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/vax/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot.mop

MDEXT=	bsd bsd.rd boot.mop

.PHONY: bsd bootblocks

#	$OpenBSD: Makefile.inc,v 1.6 2009/04/21 09:12:27 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	bsd bsd.rd cd${OSrev}.iso

.PHONY: bsd

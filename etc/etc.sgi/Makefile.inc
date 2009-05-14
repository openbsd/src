#	$OpenBSD: Makefile.inc,v 1.7 2009/05/14 21:16:40 miod Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP32/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP32
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP32 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	bsd bsd.rd cd${OSrev}.iso

.PHONY: bsd

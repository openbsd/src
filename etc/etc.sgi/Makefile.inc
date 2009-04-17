#	$OpenBSD: Makefile.inc,v 1.5 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	

.PHONY: bsd


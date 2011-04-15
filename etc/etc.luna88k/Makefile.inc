#	$OpenBSD: Makefile.inc,v 1.5 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE}

MDEXT=	bsd bsd.rd

.PHONY: bsd


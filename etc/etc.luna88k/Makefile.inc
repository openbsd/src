#	$OpenBSD: Makefile.inc,v 1.4 2009/04/21 09:12:26 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

MDEXT=	bsd bsd.rd

.PHONY: bsd


#	$OpenBSD: Makefile.inc,v 1.3 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

.PHONY: bsd


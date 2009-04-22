#	$OpenBSD: Makefile.inc,v 1.13 2009/04/22 16:16:22 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	bsd bsd.rd 				# utils??

.PHONY: bsd

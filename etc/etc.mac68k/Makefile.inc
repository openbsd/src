#	$OpenBSD: Makefile.inc,v 1.14 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

MDEXT=	bsd bsd.rd 				# utils??

.PHONY: bsd

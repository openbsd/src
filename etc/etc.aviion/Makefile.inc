#	$OpenBSD: Makefile.inc,v 1.5 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/aviion/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/aviion/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/aviion/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd bsd.rd	# bootblocks?

.PHONY: bsd bootblocks

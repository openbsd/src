#	$OpenBSD: Makefile.inc,v 1.4 2009/04/21 09:12:26 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/aviion/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/aviion/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/aviion/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd bsd.rd	# bootblocks?

.PHONY: bsd bootblocks

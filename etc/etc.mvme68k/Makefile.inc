#	$OpenBSD: Makefile.inc,v 1.12 2010/08/17 19:17:14 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd bsd.rd \
	installboot netboot rboot sboot stboot bootsd bootst bootxx

.PHONY: bsd bootblocks

#	$OpenBSD: Makefile.inc,v 1.20 2009/04/17 03:58:53 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/hp300/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hp300/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/uboot.lif ${RELEASEDIR}/SYS_UBOOT
	cp ${DESTDIR}/usr/mdec/cdboot.lif ${RELEASEDIR}/SYS_CDBOOT

MDEXT=	

.PHONY: bsd bootblocks

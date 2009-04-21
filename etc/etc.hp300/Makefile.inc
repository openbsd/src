#	$OpenBSD: Makefile.inc,v 1.21 2009/04/21 09:12:26 deraadt Exp $

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

MDEXT=	bsd bsd.rd SYS_UBOOT SYS_CDBOOT

.PHONY: bsd bootblocks

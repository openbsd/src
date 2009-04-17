#	$OpenBSD: Makefile.inc,v 1.16 2009/04/17 03:58:53 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/alpha/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/alpha/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/alpha/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/netboot ${RELEASEDIR}

MDEXT=	cd${OSrev}.iso floppy${OSrev}.fs floppyB${OSrev}.fs \
	boot bootxx netboot

.PHONY: bsd bootblocks

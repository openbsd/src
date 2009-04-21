#	$OpenBSD: Makefile.inc,v 1.17 2009/04/21 09:12:26 deraadt Exp $

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

MDEXT=	bsd bsd.rd cd${OSrev}.iso floppy${OSrev}.fs floppyB${OSrev}.fs \
	boot bootxx netboot

.PHONY: bsd bootblocks

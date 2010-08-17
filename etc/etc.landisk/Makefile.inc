#	$OpenBSD: Makefile.inc,v 1.6 2010/08/17 19:17:14 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/landisk/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/landisk/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/landisk/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/mbr ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd mbr bsd.rd xxboot boot miniroot${OSrev}.fs

.PHONY: bsd bootblocks

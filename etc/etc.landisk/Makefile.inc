#	$OpenBSD: Makefile.inc,v 1.3 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/landisk/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/landisk/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/landisk/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# probably will be replaced by procedure to build 'miniroot' image
bootblocks:
	cp ${DESTDIR}/usr/mdec/mbr ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	

.PHONY: bsd bootblocks

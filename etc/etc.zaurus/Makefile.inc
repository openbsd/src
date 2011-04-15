#	$OpenBSD: Makefile.inc,v 1.10 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/zaurus/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${RELEASEDIR}

MDEXT=	bsd bsd.rd zboot zbsdmod.o openbsd${OSrev}_arm.ipk Packages

.PHONY: bsd bootblocks

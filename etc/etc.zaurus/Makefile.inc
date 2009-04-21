#	$OpenBSD: Makefile.inc,v 1.8 2009/04/21 09:12:29 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/zaurus/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${RELEASEDIR}

MDEXT=	bsd bsd.rd zboot zbsdmod.o openbsd45_arm.ipk Packages

.PHONY: bsd bootblocks

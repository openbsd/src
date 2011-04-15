#	$OpenBSD: Makefile.inc,v 1.2 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/palm/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/palm/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/palm/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${RELEASEDIR}

MDEXT=	bsd bsd.rd Packages

.PHONY: bsd bootblocks

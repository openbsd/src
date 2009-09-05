#	$OpenBSD: Makefile.inc,v 1.1 2009/09/05 02:19:09 marex Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/palm/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/palm/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/palm/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${RELEASEDIR}

MDEXT=	bsd bsd.rd Packages

.PHONY: bsd bootblocks

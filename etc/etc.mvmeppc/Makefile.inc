#	$OpenBSD: Makefile.inc,v 1.7 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvmeppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}/bootxx
	cp ${DESTDIR}/usr/mdec/bootsd ${RELEASEDIR}/bootsd
	cp ${DESTDIR}/usr/mdec/installboot ${RELEASEDIR}/installboot

MDEXT=	bsd bsd.rd bootxx bootsd installboot

.PHONY: bsd bootblocks

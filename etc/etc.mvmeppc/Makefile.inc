#	$OpenBSD: Makefile.inc,v 1.5 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvmeppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}/bootxx
	cp ${DESTDIR}/usr/mdec/bootsd ${RELEASEDIR}/bootsd
	cp ${DESTDIR}/usr/mdec/installboot ${RELEASEDIR}/installboot

MDEXT=	

.PHONY: bsd bootblocks

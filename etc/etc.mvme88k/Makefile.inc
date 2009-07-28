#	$OpenBSD: Makefile.inc,v 1.11 2009/07/28 19:30:07 miod Exp $

kernels: bsd bsd.mp bootblocks
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd bsd.mp bsd.rd
MDEXT+=	bootsd bootst bootxx installboot netboot stboot tftpboot

.PHONY: bsd bsd.mp bootblocks

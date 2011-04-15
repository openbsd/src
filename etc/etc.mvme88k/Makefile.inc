#	$OpenBSD: Makefile.inc,v 1.13 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bsd.mp bootblocks
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd bsd.mp bsd.rd \
	bootsd bootst bootxx installboot netboot stboot tftpboot

.PHONY: bsd bsd.mp bootblocks

#	$OpenBSD: Makefile.inc,v 1.10 2010/01/19 19:55:53 miod Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP27/bsd \
	    ${RELEASEDIR}/bsd.IP27
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP30/bsd \
	    ${RELEASEDIR}/bsd.IP30
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP30.MP/bsd \
	    ${RELEASEDIR}/bsd.mp.IP30
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP32/bsd \
	    ${RELEASEDIR}/bsd.IP32

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP27
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP27 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP30
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP30 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP30.MP
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP30.MP && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP32
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP32 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot32 ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/boot64 ${RELEASEDIR}

MDEXT=	bsd.IP27 bsd.rd.IP27 bsd.IP30 bsd.mp.IP30 bsd.rd.IP30 \
	bsd.IP32 bsd.rd.IP32 \
	boot32 boot64 cd${OSrev}.iso

.PHONY: bsd bootblocks

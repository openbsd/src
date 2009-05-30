#	$OpenBSD: Makefile.inc,v 1.8 2009/05/30 04:01:29 miod Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP27/bsd \
	    ${RELEASEDIR}/bsd.IP27
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP30/bsd \
	    ${RELEASEDIR}/bsd.IP30
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP32/bsd \
	    ${RELEASEDIR}/bsd.IP32

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP27
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP27 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP30
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP30 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC-IP32
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC-IP32 && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	bsd.IP27 bsd.rd.IP27 bsd.IP30 bsd.rd.IP30 bsd.IP32 bsd.rd.IP32 \
	cd${OSrev}.iso

.PHONY: bsd

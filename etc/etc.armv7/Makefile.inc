#	$OpenBSD: Makefile.inc,v 1.4 2013/11/01 20:35:52 jasper Exp $

ALLSOC=IMX OMAP SUNXI
KERNELS=
MDEXT=

.for SOC in ${ALLSOC}
KERNELS+=	GENERIC-${SOC} bsd.${SOC}
MDEXT+=		bsd.${SOC}.umg
MDEXT+=		bsd.rd.${SOC}
MDEXT+=		bsd.rd.${SOC}.umg
.endfor

KERNADDR_OMAP=0x80300000
KERNADDR_IMX=0x10800000
KERNADDR_SUNXI=0x40800000

.for CONF K in ${KERNELS}
.  if !target($K)
$K:
	cd ../sys/arch/${MACHINE}/conf && config ${CONF}
	cd ../sys/arch/${MACHINE}/compile/${CONF} && \
	    ${MAKE} clean && exec ${MAKE}
.  endif
ALL_KERNELS += $K
.endfor

kernels: bootblocks ${ALL_KERNELS}
.for CONF K in ${KERNELS}
	cp ../sys/arch/${MACHINE}/compile/${CONF}/bsd ${RELEASEDIR}/$K
.endfor
.for SOC in ${ALLSOC}
	mkuboot -a arm -o linux -e ${KERNADDR_${SOC}} -l ${KERNADDR_${SOC}} \
	    ${RELEASEDIR}/bsd.${SOC} ${RELEASEDIR}/bsd.${SOC}.umg
.endfor

ALL_KERNELS=

bootblocks:

MDEXT+=	miniroot-am335x-${OSrev}.fs miniroot-beagle-${OSrev}.fs \
	miniroot-panda-${OSrev}.fs

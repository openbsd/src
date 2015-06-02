#	$OpenBSD: Makefile.inc,v 1.9 2015/06/02 01:48:25 jsg Exp $

ALLSOC=IMX OMAP SUNXI
MDEXT=		bsd.rd

.for SOC in ${ALLSOC}
MDEXT+=		bsd.${SOC}.umg
MDEXT+=		bsd.rd.${SOC}.umg
.endfor

KERNADDR_OMAP=0x80300000
KERNADDR_IMX=0x10300000
KERNADDR_SUNXI=0x40300000

bootblocks:

kernels: bootblocks ${ALL_KERNELS}
.for CONF K in ${KERNELS}
	cp ../sys/arch/${MACHINE}/compile/${CONF}/bsd ${RELEASEDIR}/$K
.for SOC in ${ALLSOC}
	mkuboot -a arm -o linux -e ${KERNADDR_${SOC}} -l ${KERNADDR_${SOC}} \
	    ${RELEASEDIR}/$K ${RELEASEDIR}/$K.${SOC}.umg
.endfor
.endfor

MDEXT+=	miniroot-am335x-${OSrev}.fs miniroot-beagle-${OSrev}.fs \
	miniroot-cubie-${OSrev}.fs miniroot-cubox-${OSrev}.fs \
	miniroot-nitrogen-${OSrev}.fs miniroot-panda-${OSrev}.fs

#	$OpenBSD: Makefile.inc,v 1.2 2013/09/07 23:04:44 patrick Exp $

ALLSOC=IMX OMAP
KERNELS=
MDEXT=

.for SOC in ${ALLSOC}
KERNELS+=	GENERIC-${SOC} bsd.${SOC}
MDEXT+=		bsd.rd.${SOC}
.endfor

bootblocks:

MDEXT+=	miniroot-am335x-${OSrev}.fs miniroot-beagle-${OSrev}.fs \
	miniroot-panda-${OSrev}.fs

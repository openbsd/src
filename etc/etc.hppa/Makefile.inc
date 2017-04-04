#	$OpenBSD: Makefile.inc,v 1.13 2017/04/04 00:03:12 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso lif${OSrev}.fs xxboot

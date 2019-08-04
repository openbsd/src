#	$OpenBSD: Makefile.inc,v 1.6 2019/08/04 08:53:14 visa Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	boot bsd.rd miniroot${OSrev}.fs

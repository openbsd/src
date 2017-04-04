#	$OpenBSD: Makefile.inc,v 1.10 2017/04/04 00:03:12 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	miniroot${OSrev}.fs \
	boot

#	$OpenBSD: Makefile.inc,v 1.9 2014/02/27 18:55:39 miod Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	miniroot${OSrev}.fs \
	boot

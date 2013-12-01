#	$OpenBSD: Makefile.inc,v 1.8 2013/12/01 20:41:25 miod Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	boot

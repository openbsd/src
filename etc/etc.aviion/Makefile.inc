#	$OpenBSD: Makefile.inc,v 1.8 2013/10/15 13:28:08 miod Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/

MDEXT=	bsd.rd bsd.rd.xcf \
	boot

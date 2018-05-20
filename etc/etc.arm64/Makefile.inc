#	$OpenBSD: Makefile.inc,v 1.4 2018/02/21 00:43:03 jsg Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTAA64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTAA64.EFI miniroot${OSrev}.fs

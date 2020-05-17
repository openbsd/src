#	$OpenBSD: Makefile.inc,v 1.5 2020/05/17 17:04:28 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTAA64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTAA64.EFI miniroot${OSrev}.img

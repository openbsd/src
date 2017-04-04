#	$OpenBSD: Makefile.inc,v 1.3 2017/04/04 00:03:12 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTAA64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTAA64.EFI miniroot${OSrev}.fs

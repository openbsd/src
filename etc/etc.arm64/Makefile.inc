#	$OpenBSD: Makefile.inc,v 1.2 2017/02/11 06:36:03 jsg Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/BOOTAA64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTAA64.EFI miniroot${OSrev}.fs

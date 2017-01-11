#	$OpenBSD: Makefile.inc,v 1.1 2017/01/11 17:05:29 patrick Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/BOOTAA64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTAA64.EFI

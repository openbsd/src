#	$OpenBSD: Makefile.inc,v 1.12 2011/07/22 09:30:54 espie Exp $

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot.mop

MDEXT=	bsd.rd boot.mop cd${OSrev}.iso floppy${OSrev}.fs

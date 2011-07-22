#	$OpenBSD: Makefile.inc,v 1.8 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso lif${OSrev}.fs xxboot

#	$OpenBSD: Makefile.inc,v 1.5 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd.rd boot miniroot${OSrev}.fs

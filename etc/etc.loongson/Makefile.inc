#	$OpenBSD: Makefile.inc,v 1.6 2017/04/04 00:03:12 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd.rd boot miniroot${OSrev}.fs

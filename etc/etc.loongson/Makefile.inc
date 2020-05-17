#	$OpenBSD: Makefile.inc,v 1.7 2020/05/17 17:04:29 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd.rd boot miniroot${OSrev}.img

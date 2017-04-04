#	$OpenBSD: Makefile.inc,v 1.9 2017/04/04 00:03:12 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/mbr ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	mbr bsd.rd xxboot boot miniroot${OSrev}.fs

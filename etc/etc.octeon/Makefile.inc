#	$OpenBSD: Makefile.inc,v 1.1 2013/03/26 17:58:11 jasper Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot

MDEXT=	bsd.rd

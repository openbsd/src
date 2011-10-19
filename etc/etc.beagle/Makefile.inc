#	$OpenBSD: Makefile.inc,v 1.1 2011/10/19 02:24:31 uwe Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd.rd boot

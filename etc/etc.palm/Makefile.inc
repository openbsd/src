#	$OpenBSD: Makefile.inc,v 1.3 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${RELEASEDIR}

MDEXT=	bsd.rd Packages

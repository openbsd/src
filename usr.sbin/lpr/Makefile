#	$OpenBSD: Makefile,v 1.4 2001/12/06 03:12:30 ericj Exp $

SUBDIR=	lpc lpd lpq lpr lprm lptest pac filters

.if make(install)
SUBDIR+= SMM.doc
.endif

CFLAGS+=	-Wall
.include <bsd.subdir.mk>

#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.2 1997/01/17 15:54:14 millert Exp $

SUBDIR=	lpc lpd lpq lpr lprm lptest pac filters

.if make(install)
SUBDIR+= SMM.doc
.endif

.include <bsd.subdir.mk>

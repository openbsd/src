#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:47:39 deraadt Exp $

SUBDIR=	lpc lpd lpq lpr lprm lptest pac filters

.if make(install)
SUBDIR+= SMM.doc
.endif

.include <bsd.subdir.mk>

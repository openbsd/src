#	$OpenBSD: Makefile,v 1.3 2005/04/27 18:02:16 deraadt Exp $
#	$NetBSD: Makefile,v 1.5 2002/12/13 02:36:37 lukem Exp $

MAN=	sgivol.8
MANSUBDIR=sgi

.if ${MACHINE} == "sgi"
PROG=	sgivol
LDADD+=	-lutil
LDSTATIC=-static

.else
NOPROG=
.endif

.include <bsd.prog.mk>

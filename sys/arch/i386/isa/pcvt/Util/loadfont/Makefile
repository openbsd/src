#	$OpenBSD: Makefile,v 1.5 2000/05/27 05:52:12 deraadt Exp $

MAN1=	loadfont.1
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	loadfont
.else
NOPROG=	yes
.endif

.include <bsd.prog.mk>

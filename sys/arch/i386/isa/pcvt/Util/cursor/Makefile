#	$OpenBSD: Makefile,v 1.5 2000/05/27 05:52:11 deraadt Exp $

MAN1=cursor.1
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	cursor
.else
NOPROG=yes
.endif

.include <bsd.prog.mk>

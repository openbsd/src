#	$OpenBSD: Makefile,v 1.5 2000/05/19 14:49:43 espie Exp $

.if ${MACHINE} == "i386"
PROG=	fed
SRCS=	fed.c select.c edit.c misc.c
LDADD=	-lcurses
.else
NOPROG=	yes
.endif
MAN=	fed.1
MANSUBDIR=i386

.include <bsd.prog.mk>

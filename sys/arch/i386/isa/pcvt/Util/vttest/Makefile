#	$OpenBSD: Makefile,v 1.3 2000/05/19 14:49:45 espie Exp $

.if ${MACHINE} == "i386"
PROG=		vttest
CFLAGS+=	-traditional -DUSEMYSTTY
SRCS=		main.c esc.c
.else
NOPROG=yes
.endif

MAN=		vttest.1
MANSUBDIR=	i386

.include <bsd.prog.mk>

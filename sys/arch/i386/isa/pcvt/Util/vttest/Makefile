#	$OpenBSD: Makefile,v 1.4 2000/05/25 21:54:56 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=		vttest
CFLAGS+=	-traditional -DUSEMYSTTY
SRCS=		main.c esc.c
.else
NOPROG=yes
.endif

MAN=		vttest.1
MANSUBDIR=	/i386

.include <bsd.prog.mk>

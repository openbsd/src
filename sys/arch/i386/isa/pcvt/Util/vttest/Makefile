#	$OpenBSD: Makefile,v 1.5 2000/05/27 05:52:12 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=		vttest
CFLAGS+=	-traditional -DUSEMYSTTY
SRCS=		main.c esc.c
.else
NOPROG=yes
.endif

MAN1=		vttest.1
MANSUBDIR=	/i386

.include <bsd.prog.mk>

#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:36:52 deraadt Exp $

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}
.if (${MACHINE} == "i386") || (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

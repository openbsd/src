#	$OpenBSD: Makefile,v 1.5 1998/07/06 13:42:24 pefo Exp $

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}

.if (${MACHINE} == "i386")
CFLAGS+=-DHAVEPCVT
.endif

.if (${MACHINE} == "i386") || (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

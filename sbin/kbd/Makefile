#	$OpenBSD: Makefile,v 1.7 1999/07/20 15:10:51 deraadt Exp $

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}

.if (${MACHINE} == "i386")
CFLAGS+=-DHAVEPCVT
.endif

.if (${MACHINE} == "i386") || (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.elif (${MACHINE} == "sparc")
SRCS+=	kbd_sparc.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

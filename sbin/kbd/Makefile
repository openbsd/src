#	$OpenBSD: Makefile,v 1.9 2001/03/08 08:00:16 maja Exp $

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}

.if (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.elif (${MACHINE} == "i386") || (${MACHINE} == "powerpc")
SRCS+=	kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.elif (${MACHINE} == "sparc")
SRCS+=	kbd_sparc.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

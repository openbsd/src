#	$OpenBSD: Makefile,v 1.13 2002/02/19 01:49:57 maja Exp $

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}

.if (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.elif (${MACHINE} == "i386") || (${MACHINE} == "macppc")  || \
      (${MACHINE} == "vax") || (${MACHINE} == "alpha") || \
      (${MACHINE} == "sparc64")
SRCS+=	kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.elif (${MACHINE} == "sparc")
SRCS+=	kbd_sparc.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

#	$OpenBSD: Makefile,v 1.10 2001/06/04 20:53:22 maja Exp $

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}

.if (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.elif (${MACHINE} == "i386") || (${MACHINE} == "powerpc")  || \
      (${MACHINE} == "vax")
SRCS+=	kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.elif (${MACHINE} == "sparc")
SRCS+=	kbd_sparc.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

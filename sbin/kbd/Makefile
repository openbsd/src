#	$OpenBSD: Makefile,v 1.2 1996/04/29 11:21:08 mickey Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	kbd
MAN=	kbd.8

SRCS=	main.c
CFLAGS+=-I${.CURDIR}
.if ${MACHINE} == "i386"
SRCS+=	kbd_i386.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

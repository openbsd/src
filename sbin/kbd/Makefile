#	$OpenBSD: Makefile,v 1.3 1996/09/28 08:49:07 pefo Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

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

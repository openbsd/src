#	$OpenBSD: Makefile,v 1.1 2002/04/12 02:15:31 deraadt Exp $

PROG=	kbd
MAN=
COPTS+=	-Os
CFLAGS+=-DNOKVM -I${.CURDIR}/../../../sbin/kbd
.PATH:  ${.CURDIR}/../../../sbin/kbd

SRCS=	main.c

.if (${MACHINE} == "arc")
SRCS+=	kbd_i386.c
.elif (${MACHINE} == "i386") || (${MACHINE} == "macppc")  || \
      (${MACHINE} == "vax") || (${MACHINE} == "alpha") || \
      (${MACHINE} == "sparc64")
SRCS+=	kbd_wscons.c
.elif (${MACHINE} == "sparc")
SRCS+=	kbd_sparc.c
.else
SRCS+=	kbd_void.c
.endif

.include <bsd.prog.mk>

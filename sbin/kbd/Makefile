#	$OpenBSD: Makefile,v 1.14 2002/04/19 00:05:20 miod Exp $

.if (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax")
PROG=	kbd

SRCS=	main.c
CFLAGS+=-I${.CURDIR}

.if (${MACHINE} == "sparc")
SRCS+=	kbd_sparc.c
.else
SRCS+=	kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}
.endif

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sparc sparc64 vax

.include <bsd.prog.mk>

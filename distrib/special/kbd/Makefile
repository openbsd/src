#	$OpenBSD: Makefile,v 1.2 2002/04/20 00:27:54 miod Exp $

.if (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax")
PROG=	kbd
COPTS+=	-Os

SRCS=	main.c
CFLAGS+=-DNOKVM -I${.CURDIR}/../../../sbin/kbd
.PATH:  ${.CURDIR}/../../../sbin/kbd

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

MAN=

.include <bsd.prog.mk>

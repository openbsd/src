#	$OpenBSD: Makefile,v 1.3 2002/08/12 10:46:52 miod Exp $

.if (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax")
PROG=	kbd
COPTS+=	-Os

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM -I${.CURDIR}/../../../sbin/kbd
.PATH:  ${.CURDIR}/../../../sbin/kbd

LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=

.include <bsd.prog.mk>

#	$OpenBSD: Makefile,v 1.7 2004/02/07 21:07:16 deraadt Exp $

.if (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax") || (${MACHINE_ARCH} == "x86_64") || \
    (${MACHINE} == "cats") || (${MACHINE} == "pegasos")
PROG=	kbd
COPTS+=	-Os

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM
.PATH:  ${.CURDIR}/../../../sbin/kbd

LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=

.include <bsd.prog.mk>

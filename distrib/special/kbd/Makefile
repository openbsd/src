#	$OpenBSD: Makefile,v 1.9 2004/05/03 15:18:19 drahn Exp $

.if (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "cats")
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

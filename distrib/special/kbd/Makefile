#	$OpenBSD: Makefile,v 1.12 2006/01/04 20:40:00 miod Exp $

.if ${MACHINE} != "mvme68k" && ${MACHINE} != "mvme88k"
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

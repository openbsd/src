#	$OpenBSD: Makefile,v 1.13 2006/05/09 21:17:13 miod Exp $

.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k"
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

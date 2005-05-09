#	$OpenBSD: Makefile,v 1.11 2005/05/09 20:23:42 miod Exp $

.if ${MACHINE} != "mac68k" && ${MACHINE} != "mvme68k" && \
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

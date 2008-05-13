#	$OpenBSD: Makefile,v 1.14 2008/05/13 21:29:32 miod Exp $

.if ${MACHINE} != "aviion" && ${MACHINE} != "landisk" && \
    ${MACHINE} != "mvme68k" && ${MACHINE} != "mvme88k" && \
    ${MACHINE} != "socppc"
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

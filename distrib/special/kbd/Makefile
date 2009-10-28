#	$OpenBSD: Makefile,v 1.16 2009/10/28 07:36:49 deraadt Exp $

.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k"
PROG=	kbd

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM
.PATH:  ${.CURDIR}/../../../sbin/kbd

LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

.include <bsd.prog.mk>

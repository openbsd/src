#	$OpenBSD: Makefile,v 1.6 2002/05/11 00:20:20 espie Exp $

LDSTATIC=	${STATIC}
PROG=	encrypt
SRCS= encrypt.c pwd_gensalt.c
.PATH: ${.CURDIR}/../passwd

LDADD = -lutil
DPADD= ${LIBUTIL}
LIBEXEC?=       /usr/libexec

LINKS=	${BINDIR}/encrypt ${LIBEXEC}/makekey
MLINKS=	encrypt.1 makekey.8

.include <bsd.prog.mk>

#	$OpenBSD: Makefile,v 1.4 1997/03/30 19:22:42 provos Exp $

PROG=	encrypt
SRCS= encrypt.c pwd_gensalt.c
.PATH: ${.CURDIR}/../passwd

LDADD = -lutil
LIBEXEC?=       /usr/libexec

LINKS=	${BINDIR}/encrypt ${LIBEXEC}/makekey
MLINKS=	encrypt.1 makekey.8

.include <bsd.prog.mk>

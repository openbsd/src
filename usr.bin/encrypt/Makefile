#	$OpenBSD: Makefile,v 1.5 1998/04/11 09:46:38 deraadt Exp $

LDSTATIC=	${STATIC}
PROG=	encrypt
SRCS= encrypt.c pwd_gensalt.c
.PATH: ${.CURDIR}/../passwd

LDADD = -lutil
LIBEXEC?=       /usr/libexec

LINKS=	${BINDIR}/encrypt ${LIBEXEC}/makekey
MLINKS=	encrypt.1 makekey.8

.include <bsd.prog.mk>

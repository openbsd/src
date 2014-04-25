# $OpenBSD: Makefile,v 1.4 2014/04/25 12:20:17 mikeb Exp $

PROG	= gzsig
SRCS	= gzsig.c key.c sign.c ssh.c ssh2.c util.c verify.c x509.c

LDADD	= -lcrypto -lm
DPADD	= ${LIBCRYPTO} ${LIBM}

.include <bsd.prog.mk>

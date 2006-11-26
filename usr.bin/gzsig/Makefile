# $OpenBSD: Makefile,v 1.3 2006/11/26 11:31:09 deraadt Exp $

PROG	= gzsig
SRCS	= gzsig.c key.c sign.c ssh.c ssh2.c util.c verify.c x509.c

LDADD	= -lcrypto -lm
DPADD	= ${LIBCRYPTO} ${LIBM}

CLEANFILES	+= TAGS *~

.include <bsd.prog.mk>

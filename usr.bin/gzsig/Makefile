# $OpenBSD: Makefile,v 1.2 2005/05/29 02:41:42 marius Exp $

PROG	= gzsig
SRCS	= gzsig.c key.c sign.c ssh.c ssh2.c util.c verify.c x509.c

LDADD	= -lcrypto -lm

CLEANFILES	+= TAGS *~

.include <bsd.prog.mk>

#	$OpenBSD: Makefile,v 1.4 2006/06/30 16:09:27 tedu Exp $

PROG=	vnconfig
MAN=	vnconfig.8
DPADD=	${LIBUTIL}
LDADD=	-lutil -lcrypto
SRCS=vnconfig.c pkcs5_pbkdf2.c

.include <bsd.prog.mk>

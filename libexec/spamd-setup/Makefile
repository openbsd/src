#	$OpenBSD: Makefile,v 1.3 2004/01/21 08:07:41 deraadt Exp $

PROG=	spamd-setup
SRCS=	spamd-setup.c
MAN=	spamd-setup.8

LDADD=	-lz
DPADD=	${LIBZ}

CFLAGS+= -Wall -Wstrict-prototypes -ansi

.include <bsd.prog.mk>

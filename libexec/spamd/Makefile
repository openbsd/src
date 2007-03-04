#	$OpenBSD: Makefile,v 1.9 2007/03/04 03:19:41 beck Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c grey.c sync.c
MAN=	spamd.8

CFLAGS+= -Wall -Wstrict-prototypes

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

.include <bsd.prog.mk>

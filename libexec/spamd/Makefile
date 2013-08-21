#	$OpenBSD: Makefile,v 1.10 2013/08/21 16:13:29 millert Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c gdcopy.c grey.c sync.c
MAN=	spamd.8

CFLAGS+= -Wall -Wstrict-prototypes

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

.include <bsd.prog.mk>

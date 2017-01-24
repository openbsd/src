#	$OpenBSD: Makefile,v 1.2 2017/01/24 09:25:27 deraadt Exp $

PROG=	ocspcheck
MAN=	ocspcheck.8

LDADD=	-ltls -lssl -lcrypto
DPADD=	${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

CFLAGS+= -Wall -Werror
CFLAGS+= -Wformat
CFLAGS+= -Wformat-security
CFLAGS+= -Wimplicit
CFLAGS+= -Wreturn-type
CFLAGS+= -Wshadow
CFLAGS+= -Wtrigraphs
CFLAGS+= -Wuninitialized
CFLAGS+= -Wunused

CFLAGS+= -DLIBRESSL_INTERNAL

SRCS=	ocspcheck.c http.c

.include <bsd.prog.mk>

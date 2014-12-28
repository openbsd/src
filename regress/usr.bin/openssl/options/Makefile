#	$OpenBSD: Makefile,v 1.1 2014/12/28 14:01:33 jsing Exp $

PROG=	optionstest
SRCS=	optionstest.c

LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}

OSSLSRC= ${.CURDIR}/../../../../usr.bin/openssl/
CFLAGS+= -I${OSSLSRC}

.PATH:	${OSSLSRC}
SRCS+=	apps.c

CFLAGS+= -Werror

.include <bsd.regress.mk>

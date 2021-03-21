#	$OpenBSD: Makefile,v 1.2 2021/03/21 17:22:37 jsing Exp $

PROG=	optionstest
SRCS=	optionstest.c

LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}

OSSLSRC= ${.CURDIR}/../../../../usr.bin/openssl/
CFLAGS+= -DLIBRESSL_INTERNAL -I${OSSLSRC}

.PATH:	${OSSLSRC}
SRCS+=	apps.c

CFLAGS+= -Werror

.include <bsd.regress.mk>

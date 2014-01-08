#	$OpenBSD: Makefile,v 1.2 2014/01/08 03:59:46 tedu Exp $

.PATH: ${.CURDIR}/../ssh
CPPFLAGS += -I${.CURDIR}/../ssh

SRCS=	signify.c
SRCS+=	ed25519.c fe25519.c ge25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	crypto_api.c

PROG=	signify

LDADD=  -lutil
DPADD=  ${LIBUTIL}

COPTS+=	-Wall

.include <bsd.prog.mk>

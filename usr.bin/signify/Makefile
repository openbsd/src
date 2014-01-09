#	$OpenBSD: Makefile,v 1.4 2014/01/09 18:59:35 tedu Exp $

.PATH: ${.CURDIR}/../ssh
CPPFLAGS += -I${.CURDIR}/../ssh

SRCS=	signify.c
SRCS+=	fe25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	mod_ed25519.c mod_ge25519.c
SRCS+=	crypto_api.c base64.c

PROG=	signify

LDADD=  -lutil
DPADD=  ${LIBUTIL}

COPTS+=	-Wall

.include <bsd.prog.mk>

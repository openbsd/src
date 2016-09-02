#	$OpenBSD: Makefile,v 1.11 2016/09/02 16:10:56 espie Exp $

SRCS=	signify.c
SRCS+=	zsig.c
SRCS+=	fe25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	mod_ed25519.c mod_ge25519.c
SRCS+=	crypto_api.c

PROG=	signify

LDADD=  -lutil
DPADD=  ${LIBUTIL}

COPTS+=	-Wall

.include <bsd.prog.mk>

#	$OpenBSD: Makefile,v 1.2 2015/12/08 20:13:30 tedu Exp $

.PATH: ${.CURDIR}/../../../usr.bin/signify

SRCS=	signify.c
SRCS+=	fe25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	mod_ed25519.c mod_ge25519.c
SRCS+=	crypto_api.c

PROG=	signify

LDADD=	-lutil
DPADD=	${LIBUTIL}

COPTS+=	-Wall -DVERIFYONLY

.include <bsd.prog.mk>

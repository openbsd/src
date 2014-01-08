#	$OpenBSD: Makefile,v 1.1 2014/01/08 04:33:23 deraadt Exp $

.PATH: ${.CURDIR}/../../../usr.bin/signify
.PATH: ${.CURDIR}/../../../usr.bin/ssh
CPPFLAGS += -I${.CURDIR}/../../../usr.bin/ssh

SRCS=	signify.c
SRCS+=	fe25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	mod_ed25519.c mod_ge25519.c
SRCS+=	crypto_api.c

PROG=	signify

LDADD=	-lutil
DPADD=	${LIBUTIL}

COPTS+=	-Wall -DVERIFYONLY

.include <bsd.prog.mk>

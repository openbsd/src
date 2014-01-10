#	$OpenBSD: Makefile,v 1.6 2014/01/10 04:23:37 deraadt Exp $

.PATH: ${.CURDIR}/../ssh
CPPFLAGS += -I${.CURDIR}/../ssh

SRCS=	signify.c
SRCS+=	fe25519.c sc25519.c smult_curve25519_ref.c
SRCS+=	mod_ed25519.c mod_ge25519.c
SRCS+=	crypto_api.c
.PATH: ${.CURDIR}/../../lib/libc/net
SRCS+=	base64.c

PROG=	signify

LDADD=  -lutil
DPADD=  ${LIBUTIL}

COPTS+=	-Wall

.include <bsd.prog.mk>

#       $OpenBSD: Makefile,v 1.7 2016/09/02 11:23:11 mikeb Exp $

DIR=	${.CURDIR}/../../../../sys

CFLAGS+=	-I${DIR}

PROG=   des3
SRCS=	des3.c
LDADD=-lcrypto
DPADD=${LIBCRYPTO}

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

.PATH:	${DIR}/crypto
SRCS+=	cast.c ecb_enc.c ecb3_enc.c gmac.c rijndael.c set_key.c
SRCS+=	chachapoly.c poly1305.c
SRCS+=	xform.c

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>

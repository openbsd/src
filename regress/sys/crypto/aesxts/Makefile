#       $OpenBSD: Makefile,v 1.5 2017/05/02 11:46:00 mikeb Exp $

DIR=	${.CURDIR}/../../../../sys

CFLAGS+=	-I${DIR}

PROG=   aes_xts
SRCS=	aes_xts.c

CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wshadow

REGRESS_TARGETS=	run-regress-${PROG}

.PATH:  ${DIR}/crypto
SRCS+=	cast.c ecb_enc.c ecb3_enc.c gmac.c aes.c set_key.c
SRCS+=	chachapoly.c poly1305.c
SRCS+=	xform.c

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>

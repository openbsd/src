#       $OpenBSD: Makefile,v 1.2 2013/10/06 16:59:34 jsing Exp $

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
CDIAGFLAGS+=	-Wbounded
CDIAGFLAGS+=	-Wshadow

REGRESS_TARGETS=	run-regress-${PROG}

.PATH:  ${DIR}/crypto
SRCS+=	cast.c ecb_enc.c ecb3_enc.c gmac.c rijndael.c set_key.c
SRCS+=	xform.c

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>

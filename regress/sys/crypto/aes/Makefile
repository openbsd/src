#       $OpenBSD: Makefile,v 1.4 2017/05/02 11:33:48 mikeb Exp $

DIR=	${.CURDIR}/../../../../sys

CFLAGS+=	-I${DIR}

PROG=   aestest
SRCS=	aestest.c
CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wshadow

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

.PATH:	${DIR}/crypto
SRCS+=	aes.c

run-regress-${PROG}: ${PROG}
	./${PROG} ${.CURDIR}/vectors/*.txt

.include <bsd.regress.mk>

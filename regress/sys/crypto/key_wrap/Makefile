#	$OpenBSD: Makefile,v 1.1 2008/08/12 15:49:08 damien Exp $

DIR=${.CURDIR}/../../../../sys

PROG=	key_wrap_test
SRCS+=	rijndael.c key_wrap.c key_wrap_test.c
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

CFLAGS+=	-I${DIR}

.PATH:	${DIR}/crypto/

run-regress-${PROG}: ${PROG}
	./${PROG} | diff - ${PROG}.txt

.include <bsd.regress.mk>

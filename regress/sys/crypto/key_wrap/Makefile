#	$OpenBSD: Makefile,v 1.3 2010/08/07 19:46:58 matthew Exp $

DIR=${.CURDIR}/../../../../sys

PROG=	key_wrap_test
SRCS+=	rijndael.c key_wrap.c key_wrap_test.c timingsafe_bcmp.c
CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
#CDIAGFLAGS+=	-Wshadow

REGRESS_TARGETS=	run-regress-${PROG}

CFLAGS+=	-I${DIR}

.PATH:	${DIR}/crypto/ ${DIR}/lib/libkern/

run-regress-${PROG}: ${PROG}
	./${PROG} | diff - ${.CURDIR}/${PROG}.txt

.include <bsd.regress.mk>

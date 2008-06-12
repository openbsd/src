#       $OpenBSD: Makefile,v 1.1 2008/06/12 19:42:48 djm Exp $

PROG=   aestest

CDIAGFLAGS=	-Wall
#CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
CDIAGFLAGS+=	-Wshadow

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG} ${.CURDIR}/vectors/*.txt

.include <bsd.regress.mk>

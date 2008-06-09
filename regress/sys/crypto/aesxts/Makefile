#       $OpenBSD: Makefile,v 1.1 2008/06/09 16:15:05 djm Exp $

PROG=   aes_xts

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

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>

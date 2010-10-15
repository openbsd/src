#       $OpenBSD: Makefile,v 1.5 2010/10/15 10:39:12 jsg Exp $

PROG=   des3
LDADD=-lcrypto
DPADD=${LIBCRYPTO}

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>

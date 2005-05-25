#       $OpenBSD: Makefile,v 1.1 2005/05/25 05:47:53 markus Exp $

PROG=   aesctr

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>

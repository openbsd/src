#	$OpenBSD: Makefile,v 1.3 2004/12/29 04:28:39 david Exp $

PROG=misc
CLEANFILES=mapped_file

REGRESS_ROOT_TARGETS=	run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>

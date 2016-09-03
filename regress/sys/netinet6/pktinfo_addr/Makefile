# $OpenBSD: Makefile,v 1.3 2016/09/03 15:36:33 vgross Exp $

PROG	   = runtest

REGRESS_TARGETS ?=	run-regress-runtest

run-regress-runtest: ${PROG}
	${SETENV} PROG=${.OBJDIR}/${PROG} SUDO=${SUDO} /bin/ksh ${.CURDIR}/test_wrapper.sh

.include <bsd.regress.mk>

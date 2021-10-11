# $OpenBSD: Makefile,v 1.3 2021/10/11 05:46:14 anton Exp $
# Regress tests for snmpd

REGRESS_TARGETS =	run-regress-snmpd

run-regress-snmpd:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/snmpd.sh

.include <bsd.regress.mk>

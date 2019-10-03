# $OpenBSD: Makefile,v 1.2 2019/10/03 11:05:54 martijn Exp $
# Regress tests for snmpd

REGRESS_TARGETS =	run-regress-snmpd

run-regress-snmpd:
	env OBJDIR=${.OBJDIR} ${SUDO} sh ${.CURDIR}/snmpd.sh

.include <bsd.regress.mk>

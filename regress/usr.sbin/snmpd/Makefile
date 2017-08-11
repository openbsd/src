# $OpenBSD: Makefile,v 1.1 2017/08/11 17:45:02 rob Exp $
# Regress tests for snmpd

REGRESS_TARGETS =	run-regress-snmpd

# Needs netsnmp

NETSNMP_DEP != which snmpset >/dev/null 2>&1 ; echo $?

.if ("${NETSNMP_DEP}" !=0)
run-regress-snmpd:
	@echo install netsnmp
	@echo SKIPPED
.endif

run-regress-snmpd:
	env OBJDIR=${.OBJDIR} ${SUDO} sh ${.CURDIR}/snmpd.sh

.include <bsd.regress.mk>

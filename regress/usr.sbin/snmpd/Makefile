# $OpenBSD: Makefile,v 1.4 2021/10/23 17:56:31 bluhm Exp $
# Regress tests for snmpd

REGRESS_SETUP_ONCE =	setup
REGRESS_CLEANUP =	cleanup
REGRESS_TARGETS =	run-snmpd

setup:
	${SUDO} /etc/rc.d/snmpd stop

cleanup:
	-${SUDO} /etc/rc.d/snmpd restart || true

run-snmpd:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/snmpd.sh

.include <bsd.regress.mk>

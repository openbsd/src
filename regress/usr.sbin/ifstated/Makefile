# $OpenBSD: Makefile,v 1.3 2018/02/05 18:28:15 anton Exp $
# Regress tests for ifstated

REGRESS_TARGETS =	run-regress-statemachine run-regress-ifstated

run-regress-statemachine:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/statemachine

run-regress-ifstated:
	${SUDO} env OBJDIR=${.OBJDIR} sh ${.CURDIR}/ifstated

.include <bsd.regress.mk>

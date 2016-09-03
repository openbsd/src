# $OpenBSD: Makefile,v 1.4 2016/09/03 15:59:28 vgross Exp $

PROG	 = runtest
PORT	 = 23000
IFADDR	!= route -n show -inet | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ { print $$1; exit }'
IF6ADDR	!= route -n show -inet6 | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ && $$1 !~ /^fe80/ { print $$1; exit }'

run-regress-runtest4: ${PROG}
	${.OBJDIR}/${PROG} unicast ${PORT} ${IFADDR}
	${.OBJDIR}/${PROG} mcast ${PORT} 224.1.1.2 ${IFADDR}

REGRESS_TARGETS += run-regress-runtest4

.if ! empty(IF6ADDR)
run-regress-runtest6: ${PROG}
	${.OBJDIR}/${PROG} unicast ${PORT} ${IF6ADDR}
	${SUDO} ${.OBJDIR}/${PROG} mcast ${PORT} ff1e::123 ${IF6ADDR}

REGRESS_TARGETS += run-regress-runtest6
.endif

.include <bsd.regress.mk>

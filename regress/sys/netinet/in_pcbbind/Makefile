# $OpenBSD: Makefile,v 1.5 2016/09/05 10:15:24 vgross Exp $

PROG	 = runtest
PORT	 = 23000
IFADDR	!= route -n show -inet | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ { print $$1; exit }'
IF6ADDR	!= route -n show -inet6 | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ && $$1 !~ /^fe80/ { print $$1; exit }'

run-regress-runtest4: ${PROG}
	${.OBJDIR}/${PROG} ${IFADDR} ${PORT}
	${.OBJDIR}/${PROG} 224.0.2.1 ${PORT} ${IFADDR}

REGRESS_TARGETS += run-regress-runtest4

.if ! empty(IF6ADDR)
run-regress-runtest6: ${PROG}
	${.OBJDIR}/${PROG} ${IF6ADDR} ${PORT}
	${SUDO} ${.OBJDIR}/${PROG} ff1e::123 ${PORT} ${IF6ADDR}

REGRESS_TARGETS += run-regress-runtest6
.endif

.include <bsd.regress.mk>

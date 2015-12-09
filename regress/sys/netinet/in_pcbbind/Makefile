# $OpenBSD: Makefile,v 1.2 2015/12/09 14:52:59 vgross Exp $

PROG	 = runtest
PORT	 = 23000
IFADDR	!= route -n show -inet | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ { print $$1; exit }'
IF6ADDR	!= route -n show -inet6 | awk '$$8 !~ "lo0" && $$3 ~ /U.*l/ && $$1 !~ /^fe80/ { print $$1; exit }'

run-regress-runtest: ${PROG}
	${.OBJDIR}/${PROG} unicast ${PORT} ${IFADDR} || true
	${.OBJDIR}/${PROG} mcast ${PORT} 224.1.1.2 ${IFADDR} || true
	${.OBJDIR}/${PROG} unicast ${PORT} ${IF6ADDR} || true
	${SUDO} ${.OBJDIR}/${PROG} mcast ${PORT} ff1e::123 ${IF6ADDR} || true

.include <bsd.regress.mk>

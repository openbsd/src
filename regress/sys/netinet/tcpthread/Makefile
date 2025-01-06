# $OpenBSD: Makefile,v 1.2 2025/01/06 22:25:38 bluhm Exp $

PROG=		tcpthread
LDADD=		-lpthread
DPADD=		${LIBPTHREAD}
WARNINGS=	yes

CLEANFILES=	ktrace.out

REGRESS_SETUP +=	${PROG}

REGRESS_TARGETS +=	run-default
run-default:
	time ${KTRACE} ./${PROG}

REGRESS_TARGETS +=	run-sock100
run-sock100:
	ulimit -n 500; time ${KTRACE} ./${PROG} -n100

REGRESS_TARGETS +=	run-splice
run-splice:
	time ${KTRACE} ./${PROG} -n10 -S2

REGRESS_TARGETS +=	run-unsplice
run-unsplice:
	time ${KTRACE} ./${PROG} -n10 -S2 -U1

REGRESS_TARGETS +=	run-max
run-max:
	time ${KTRACE} ./${PROG} -n10 -S4 -M100 -s2 -r2

REGRESS_TARGETS +=	run-idle
run-idle:
	time ${KTRACE} ./${PROG} -n10 -S4 -I100 -s2 -r2

REGRESS_ROOT_TARGETS +=	run-drop
REGRESS_TARGETS +=	run-drop
run-drop:
	time ${SUDO} ${KTRACE} ./${PROG} -n10 -D3 -o0

REGRESS_ROOT_TARGETS +=	run-splicedrop
REGRESS_TARGETS +=	run-splicedrop
run-splicedrop:
	time ${SUDO} ${KTRACE} ./${PROG} -n10 -S2 -D3 -o0

.include <bsd.regress.mk>

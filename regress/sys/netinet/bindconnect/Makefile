#	$OpenBSD: Makefile,v 1.1 2023/12/06 14:41:52 bluhm Exp $

PROG=		bindconnect
LDADD=		-lpthread
DPADD=		${LIBPTHREAD}
WARNINGS=	yes

CLEANFILES=	ktrace.out

${REGRESS_TARGETS}: ${PROG}

REGRESS_TARGETS +=	run-default
run-default:
	${SUDO} time ${KTRACE} ./${PROG}

REGRESS_TARGETS +=	run-bind
run-bind:
	${SUDO} time ${KTRACE} ./${PROG} -n 10 -s 2 -o 1 -b 5 -c 0

REGRESS_TARGETS +=	run-connect
run-connect:
	${SUDO} time ${KTRACE} ./${PROG} -n 10 -s 2 -o 1 -b 0 -c 5

REGRESS_TARGETS +=	run-bind-connect
run-bind-connect:
	${SUDO} time ${KTRACE} ./${PROG} -n 10 -s 2 -o 1 -b 3 -c 3

.include <bsd.regress.mk>

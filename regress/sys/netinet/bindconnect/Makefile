#	$OpenBSD: Makefile,v 1.2 2023/12/06 22:57:14 bluhm Exp $

PROG=		bindconnect
LDADD=		-lpthread
DPADD=		${LIBPTHREAD}
WARNINGS=	yes

CLEANFILES=	ktrace.out

LOCAL_NET ?=

REGRESS_ROOT_TARGETS +=	setup-maxfiles run-100000 run-localnet-connect-delete

REGRESS_SETUP_ONCE +=	setup-maxfiles
setup-maxfiles:
	[[ $$(sysctl -n kern.maxfiles) -ge 110000 ]] || \
	    ${SUDO} sysctl kern.maxfiles=110000

REGRESS_SETUP +=	${PROG}

.if ! empty(LOCAL_NET)
REGRESS_CLEANUP +=	cleanup-delete
.endif
cleanup-delete:
	-${SUDO} time ./${PROG} -s 0 -o 0 -b 0 -c 0 -d 1 -N ${LOCAL_NET} -t 1

REGRESS_TARGETS +=	run-default
run-default:
	time ${KTRACE} ./${PROG}

REGRESS_TARGETS +=	run-bind
run-bind:
	time ${KTRACE} ./${PROG} -n 16 -s 2 -o 1 -b 5 -c 0

REGRESS_TARGETS +=	run-connect
run-connect:
	time ${KTRACE} ./${PROG} -n 16 -s 2 -o 1 -b 0 -c 5

REGRESS_TARGETS +=	run-bind-connect
run-bind-connect:
	time ${KTRACE} ./${PROG} -n 16 -s 2 -o 1 -b 3 -c 3

REGRESS_TARGETS +=	run-100000
run-100000:
	${SUDO} time ${KTRACE} ./${PROG} -n 100000 -s 2 -o 1 -b 3 -c 3

REGRESS_TARGETS +=	run-reuseport
run-reuseport:
	time ${KTRACE} ./${PROG} -n 16 -s 2 -o 1 -b 3 -c 3 -r

.if empty(LOCAL_NET)
REGRESS_SKIP_TARGETS +=	run-localnet-connect run-localnet-bind-connect \
			run-localnet-connect-delete
.endif

REGRESS_TARGETS +=	run-localnet-connect
run-localnet-connect:
	time ${KTRACE} ./${PROG} -n 16 -s 2 -o 1 -c 5 -N ${LOCAL_NET}

REGRESS_TARGETS +=	run-localnet-bind-connect
run-localnet-bind-connect:
	time ${KTRACE} ./${PROG} -n 16 -s 2 -o 1 -b 3 -c 3 -N ${LOCAL_NET}

REGRESS_TARGETS +=	run-localnet-connect-delete
run-localnet-connect-delete:
	${SUDO} time ${KTRACE} \
	    ./${PROG} -n 16 -s 2 -o 1 -b 0 -c 5 -d 3 -N ${LOCAL_NET}

.include <bsd.regress.mk>

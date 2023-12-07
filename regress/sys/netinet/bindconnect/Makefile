#	$OpenBSD: Makefile,v 1.3 2023/12/07 23:47:48 bluhm Exp $

PROG=		bindconnect
LDADD=		-lpthread
DPADD=		${LIBPTHREAD}
WARNINGS=	yes

CLEANFILES=	ktrace.out

LOCAL_NET ?=
LOCAL_NET6 ?=

REGRESS_ROOT_TARGETS +=	setup-maxfiles run-100000 run-localnet-connect-delete

REGRESS_SETUP_ONCE +=	setup-maxfiles
setup-maxfiles:
	[[ $$(sysctl -n kern.maxfiles) -ge 110000 ]] || \
	    ${SUDO} sysctl kern.maxfiles=110000

REGRESS_SETUP +=	${PROG}

REGRESS_TARGETS +=	run-default
run-default:
	time ${KTRACE} ./${PROG}

NET_inet =	${LOCAL_NET}
NET_inet6 =	${LOCAL_NET6}

.for af in inet inet6

.if ! empty(NET_${af})
REGRESS_CLEANUP +=	cleanup-${af}-delete
.endif
cleanup-${af}-delete:
	-${SUDO} time ./${PROG} \
	    -f ${af} -s 0 -o 0 -b 0 -c 0 -d 1 -N ${NET_${af}} -t 1

REGRESS_TARGETS +=	run-${af}-bind
run-${af}-bind:
	time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -b 6 -c 0

REGRESS_TARGETS +=	run-${af}-connect
run-${af}-connect:
	time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -b 0 -c 6

REGRESS_TARGETS +=	run-${af}-bind-connect
run-${af}-bind-connect:
	time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -b 3 -c 3

REGRESS_TARGETS +=	run-${af}-100000
run-${af}-100000:
	${SUDO} time ${KTRACE} ./${PROG} \
	    -f ${af} -n 100000 -s 2 -o 1 -b 3 -c 3

REGRESS_TARGETS +=	run-${af}-reuseport
run-${af}-reuseport:
	time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -b 3 -c 3 -r

.if empty(NET_${af})
REGRESS_SKIP_TARGETS +=	run-${af}-localnet-connect \
			run-${af}-localnet-bind-connect \
			run-${af}-localnet-connect-delete
.endif

REGRESS_TARGETS +=	run-${af}-localnet-connect
run-${af}-localnet-connect:
	time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -c 6 -N ${NET_${af}}

REGRESS_TARGETS +=	run-${af}-localnet-bind-connect
run-${af}-localnet-bind-connect:
	time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -b 3 -c 3 -N ${NET_${af}}

REGRESS_TARGETS +=	run-${af}-localnet-connect-delete
run-${af}-localnet-connect-delete:
	${SUDO} time ${KTRACE} ./${PROG} \
	    -f ${af} -n 16 -s 2 -o 1 -b 0 -c 6 -d 3 -N ${NET_${af}}

.endfor

.include <bsd.regress.mk>

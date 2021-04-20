# $OpenBSD: Makefile,v 1.6 2021/04/20 11:17:52 martijn Exp $

.if ! exists(/usr/local/sbin/snmpd)
regress:
	@printf "Install net-snmp package to run this regress.\n"
	@printf "SKIPPED\n"
.endif

PROG=			agentx
SRCS=			main.c log.c
CFLAGS+=		-Wall
NOMAN=			yes
LDADD=			-lagentx -levent

REGRESS_SETUP_ONCE=	start
REGRESS_TARGETS=	run-regress-walk
REGRESS_CLEANUP=	stop
CLEANFILES=

SNMPD_CMD=		/usr/local/sbin/snmpd -r -C -c snmpd.conf -Le
UID!!=		id -u
.if ${UID} == 0
SNMPD_CMD+=		-u _netsnmp
.endif

CLEANFILES+=	snmpd.conf
snmpd.conf: Makefile
	@printf "agentaddress udp:127.0.0.1:6161\n" > snmpd.conf
	@printf "rwcommunity public\n" >> snmpd.conf
	@printf "master agentx\n" >> snmpd.conf
	@printf "agentXSocket agentx.sock\n" >> snmpd.conf

CLEANFILES+=	agentx.sock
start: stop snmpd.conf agentx
	${SNMPD_CMD}
	# XXX Race in net-snmpd, socket exists before data is available.
	sleep 1
	./agentx agentx.sock

stop:
	-pkill -xf "${SNMPD_CMD}"

CLEANFILES+=	walk.run.out walk.run.err
run-regress-walk:
	snmp walk 127.0.0.1:6161 openbsd > walk.run.out 2> walk.run.err || true
	diff -up ${.CURDIR}/walk.err walk.run.err
	diff -up ${.CURDIR}/walk.out walk.run.out

.include <bsd.regress.mk>

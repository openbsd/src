# $OpenBSD: Makefile,v 1.3 2021/01/13 17:00:20 martijn Exp $

.if ! exists(/usr/local/sbin/snmpd)
regress:
	@printf "Install net-snmp package to run this regress.\n"
	@printf "SKIPPED\n"
.endif

PROG=			agentx
SRCS=			main.c log.c
CFLAGS=			-Wall
NOMAN=			yes
LDADD=			-lagentx -levent

REGRESS_SETUP_ONCE=	start
REGRESS_TARGETS=	run-regress-walk
REGRESS_CLEANUP=	stop
CLEANFILES=

SNMPD_CMD=		/usr/local/sbin/snmpd -r -c ${.OBJDIR}/snmpd.conf

CLEANFILES+=	snmpd.conf
snmpd.conf:
	@printf "agentaddress udp:127.0.0.1:6161\n" > snmpd.conf
	@printf "rwcommunity public\n" >> snmpd.conf
	@printf "master agentx\n" >> snmpd.conf
	@printf "agentXSocket ${.OBJDIR}/agentx.sock\n" >> snmpd.conf

CLEANFILES+=	${.OBJDIR}/agentx.sock
start: snmpd.conf
	${SNMPD_CMD}
	${.OBJDIR}/agentx ${.OBJDIR}/agentx.sock

stop:
	pkill -xf "${SNMPD_CMD}"

CLEANFILES+=	walk.run.out walk.run.err
run-regress-walk: agentx
	snmp walk 127.0.0.1:6161 openbsd > walk.run.out 2> walk.run.err || true
	@cmp -s ${.CURDIR}/walk.err walk.run.err || \
		(printf "XXX ${*} error failed\n" && false)
	@cmp -s ${.CURDIR}/walk.out walk.run.out || \
		(printf "XXX ${*} output failed\n" && false)

.include <bsd.regress.mk>

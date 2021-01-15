# $OpenBSD: Makefile,v 1.4 2021/01/15 14:36:16 bluhm Exp $

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

SNMPD_CMD=		/usr/local/sbin/snmpd -r -c snmpd.conf -Le

CLEANFILES+=	snmpd.conf
snmpd.conf:
	@printf "agentaddress udp:127.0.0.1:6161\n" > snmpd.conf
	@printf "rwcommunity public\n" >> snmpd.conf
	@printf "master agentx\n" >> snmpd.conf
	@printf "agentXSocket ${.OBJDIR}/agentx.sock\n" >> snmpd.conf

CLEANFILES+=	agentx.sock
start: stop snmpd.conf agentx
	${SNMPD_CMD}
	./agentx agentx.sock

stop:
	-pkill -xf "${SNMPD_CMD}"

CLEANFILES+=	walk.run.out walk.run.err
run-regress-walk:
	snmp walk 127.0.0.1:6161 openbsd > walk.run.out 2> walk.run.err || true
	diff -up ${.CURDIR}/walk.err walk.run.err
	diff -up ${.CURDIR}/walk.out walk.run.out

.include <bsd.regress.mk>

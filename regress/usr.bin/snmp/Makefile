# $OpenBSD: Makefile,v 1.4 2022/02/23 10:28:05 martijn Exp $

SNMP?=			/usr/bin/snmp
SNMPD?=			/usr/sbin/snmpd -f ${.OBJDIR}/snmpd.conf

SNMPGET?=		${SNMP} get
SNMPGETNEXT?=		${SNMP} getnext
SNMPBULKGET?=		${SNMP}	bulkget
SNMPWALK?=		${SNMP} walk
SNMPBULKWALK?=		${SNMP} bulkwalk
SNMPSET?=		${SNMP} set
SNMPTRAP?=		${SNMP} trap

REGRESS_ROOT_TARGETS=	start stop
REGRESS_SETUP_ONCE=	start
REGRESS_TARGETS=
REGRESS_CLEANUP=	stop
REGRESS_SKIP_TARGETS=
CLEANFILES=

IFIDX!=			ifconfig egress | awk '/index/{print $$2}'
IFLLADDR!=		ifconfig egress | awk '/lladdr/{gsub(":", " ", $$2); print toupper($$2)}'

CLEANFILES+=		snmpd.conf
snmpd.conf: Makefile
	printf 'listen_addr="127.0.0.1"\n' > snmpd.conf
	printf 'listen6_addr="::1"\n\n' >> snmpd.conf
	printf '# Restrict daemon to listen on localhost only\n' >> snmpd.conf
	printf 'listen on $$listen_addr snmpv1 snmpv2c snmpv3\n' >> snmpd.conf
	printf 'listen on tcp $$listen_addr snmpv1 snmpv2c snmpv3\n' >> snmpd.conf
	printf 'listen on $$listen6_addr snmpv1 snmpv2c snmpv3\n' >> snmpd.conf
	printf 'listen on tcp $$listen6_addr snmpv1 snmpv2c snmpv3\n' >> snmpd.conf
	printf 'listen on $$listen_addr notify snmpv1 snmpv2c snmpv3\n\n' >> snmpd.conf
	printf 'read-only community public\n' >> snmpd.conf
	printf 'read-write community private\n' >> snmpd.conf
	printf 'trap community public\n\n' >> snmpd.conf
	printf '# (ab)use sysContact for DisplayString (255a) testing\n' >> snmpd.conf
	printf 'system contact "Reyk Fl\303\266ter"\n' >> snmpd.conf
	printf 'system services 74\n\n' >> snmpd.conf
	printf '# Provide static user-defined SNMP OIDs\n' >> snmpd.conf
	printf 'oid 1.3.6.1.4.1.30155.42.3.1 name testStringValue read-only string "Test"\n' >> snmpd.conf
	printf 'oid 1.3.6.1.4.1.30155.42.3.2 name testStringValue read-only string "abcdefghijklmnopqrstuvwxyz"\n' >> snmpd.conf
	printf 'oid 1.3.6.1.4.1.30155.42.3.3 name testIntValue read-write string a\n' >> snmpd.conf
	printf 'oid 1.3.6.1.4.1.30155.42.3.4 name testIntValue read-write integer 1\n' >> snmpd.conf
	printf '# (ab)use usmUserName for SnmpAdminString (255t) tests\n' >> snmpd.conf
	printf 'oid 1.3.6.1.6.3.15.1.2.2.1.2.1 name testStringValue read-only string "Reyk Fl\303\266ter"\n' >> snmpd.conf
	printf 'oid 1.3.6.1.6.3.15.1.2.2.1.2.2 name testStringValue read-only string "Reyk Fl\303ter"\n' >> snmpd.conf
	printf "# 256 a's\n" >> snmpd.conf
	printf 'oid 1.3.6.1.6.3.15.1.2.2.1.2.3 name testStringValue read-only string "%s"\n' "$$(jot -ba 256 | tr -d '\n')" >> snmpd.conf
	printf "# 254 a's + replacement character\n" >> snmpd.conf
	printf 'oid 1.3.6.1.6.3.15.1.2.2.1.2.4 name testStringValue read-only string "%s\357\277\275"\n\n' "$$(jot -ba 254 | tr -d '\n')" >> snmpd.conf
	printf '# Enable SNMPv3 USM with authentication, encryption and two defined users\n' >> snmpd.conf
	printf 'seclevel none\n\n' >> snmpd.conf
	printf 'user md5_des authkey testtest auth hmac-md5 enckey testtest enc des\n' >> snmpd.conf
	printf 'user md5_aes authkey testtest auth hmac-md5 enckey testtest enc aes\n' >> snmpd.conf
	printf 'user sha1_des authkey testtest auth hmac-sha1 enckey testtest enc des\n' >> snmpd.conf
	printf 'user sha1_aes authkey testtest auth hmac-sha1 enckey testtest enc aes\n' >> snmpd.conf
	printf 'user sha224_des authkey testtest auth hmac-sha224 enckey testtest enc des\n' >> snmpd.conf
	printf 'user sha224_aes authkey testtest auth hmac-sha224 enckey testtest enc aes\n' >> snmpd.conf
	printf 'user sha256_des authkey testtest auth hmac-sha256 enckey testtest enc des\n' >> snmpd.conf
	printf 'user sha256_aes authkey testtest auth hmac-sha256 enckey testtest enc aes\n' >> snmpd.conf
	printf 'user sha384_des authkey testtest auth hmac-sha384 enckey testtest enc des\n' >> snmpd.conf
	printf 'user sha384_aes authkey testtest auth hmac-sha384 enckey testtest enc aes\n' >> snmpd.conf
	printf 'user sha512_des authkey testtest auth hmac-sha512 enckey testtest enc des\n' >> snmpd.conf
	printf 'user sha512_aes authkey testtest auth hmac-sha512 enckey testtest enc aes\n\n' >> snmpd.conf
	printf 'trap handle 1.3 "${.OBJDIR}/traphandle.sh"\n' >> snmpd.conf

CLEANFILES+=		traphandle.sh
traphandle.sh: Makefile
	printf '#!/bin/sh\n\n' > traphandle.sh
	printf 'while read line; do\n' >> traphandle.sh
	printf '\tprintf "%%s\\n" "$$line" >> ${.OBJDIR}/trap_output\n' >> traphandle.sh
	printf 'done\n' >> traphandle.sh
	chmod +x traphandle.sh

CLEANFILES+=		trap_output
trap_output: Makefile
	touch trap_output
	chmod a+rw trap_output

start: stop snmpd.conf traphandle.sh trap_output
	${SUDO} ${SNMPD}

stop:
	-${SUDO} pkill -xf "${SNMPD}"

### AGENT FORMAT

REGRESS_TARGETS+=	agent_udp_v4
CLEANFILES+=		agent_udp_v4.res agent_udp_v4.exp
agent_udp_v4:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udp:127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_v4_port
CLEANFILES+=		agent_udp_v4_port.res agent_udp_v4_port.exp
agent_udp_v4_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udp:127.0.0.1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_v4_service
CLEANFILES+=		agent_udp_v4_service.res agent_udp_v4_service.exp
agent_udp_v4_service:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udp:127.0.0.1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_v4_port
CLEANFILES+=		agent_v4_port.res agent_v4_port.exp
agent_v4_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_v4_service
CLEANFILES+=		agent_v4_service.res agent_v4_service.exp
agent_v4_service:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_v4
CLEANFILES+=		agent_tcp_v4.res agent_tcp_v4.exp
agent_tcp_v4:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcp:127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_v4_port
CLEANFILES+=		agent_tcp_v4_port.res agent_tcp_v4_port.exp
agent_tcp_v4_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcp:127.0.0.1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udp6plain
CLEANFILES+=		agent_udp_udp6plain.res agent_udp_udp6plain.exp
agent_udp_udp6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udp6:::1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udp6bracket
CLEANFILES+=		agent_udp_udp6bracket.res agent_udp_udp6bracket.exp
agent_udp_udp6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udp6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udp6bracket_port
CLEANFILES+=		agent_udp_udp6bracket_port.res agent_udp_udp6bracket_port.exp
agent_udp_udp6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udp6:[::1]:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDP6plain
CLEANFILES+=		agent_udp_UDP6plain.res agent_udp_UDP6plain.exp
agent_udp_UDP6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDP6:::1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDP6bracket
CLEANFILES+=		agent_udp_UDP6bracket.res agent_udp_UDP6bracket.exp
agent_udp_UDP6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDP6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDP6bracket_port
CLEANFILES+=		agent_udp_UDP6bracket_port.res agent_udp_UDP6bracket_port.exp
agent_udp_UDP6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDP6:[::1]:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udpv6plain
CLEANFILES+=		agent_udp_udpv6plain.res agent_udp_udpv6plain.exp
agent_udp_udpv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udpv6:::1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udpv6bracket
CLEANFILES+=		agent_udp_udpv6bracket.res agent_udp_udpv6bracket.exp
agent_udp_udpv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udpv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udpv6bracket_port
CLEANFILES+=		agent_udp_udpv6bracket_port.res agent_udp_udpv6bracket_port.exp
agent_udp_udpv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udpv6:[::1]:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDPv6plain
CLEANFILES+=		agent_udp_UDPv6plain.res agent_udp_UDPv6plain.exp
agent_udp_UDPv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDPv6:::1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDPv6bracket
CLEANFILES+=		agent_udp_UDPv6bracket.res agent_udp_UDPv6bracket.exp
agent_udp_UDPv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDPv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDPv6bracket_port
CLEANFILES+=		agent_udp_UDPv6bracket_port.res agent_udp_UDPv6bracket_port.exp
agent_udp_UDPv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDPv6:[::1]:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udpipv6plain
CLEANFILES+=		agent_udp_udpipv6plain.res agent_udp_udpipv6plain.exp
agent_udp_udpipv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udpipv6:::1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udpipv6bracket
CLEANFILES+=		agent_udp_udpipv6bracket.res agent_udp_udpipv6bracket.exp
agent_udp_udpipv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udpipv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_udpipv6bracket_port
CLEANFILES+=		agent_udp_udpipv6bracket_port.res agent_udp_udpipv6bracket_port.exp
agent_udp_udpipv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic udpipv6:[::1]:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDPIPv6plain
CLEANFILES+=		agent_udp_UDPIPv6plain.res agent_udp_UDPIPv6plain.exp
agent_udp_UDPIPv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDPIPv6:::1:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDPIPv6bracket
CLEANFILES+=		agent_udp_UDPIPv6bracket.res agent_udp_UDPIPv6bracket.exp
agent_udp_UDPIPv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDPIPv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_udp_UDPIPv6bracket_port
CLEANFILES+=		agent_udp_UDPIPv6bracket_port.res agent_udp_UDPIPv6bracket_port.exp
agent_udp_UDPIPv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic UDPIPv6:[::1]:snmp sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcp6plain
CLEANFILES+=		agent_tcp_tcp6plain.res agent_tcp_tcp6plain.exp
agent_tcp_tcp6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcp6:::1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcp6bracket
CLEANFILES+=		agent_tcp_tcp6bracket.res agent_tcp_tcp6bracket.exp
agent_tcp_tcp6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcp6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcp6bracket_port
CLEANFILES+=		agent_tcp_tcp6bracket_port.res agent_tcp_tcp6bracket_port.exp
agent_tcp_tcp6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcp6:[::1]:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCP6plain
CLEANFILES+=		agent_tcp_TCP6plain.res agent_tcp_TCP6plain.exp
agent_tcp_TCP6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCP6:::1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCP6bracket
CLEANFILES+=		agent_tcp_TCP6bracket.res agent_tcp_TCP6bracket.exp
agent_tcp_TCP6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCP6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCP6bracket_port
CLEANFILES+=		agent_tcp_TCP6bracket_port.res agent_tcp_TCP6bracket_port.exp
agent_tcp_TCP6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCP6:[::1]:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcpv6plain
CLEANFILES+=		agent_tcp_tcpv6plain.res agent_tcp_tcpv6plain.exp
agent_tcp_tcpv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcpv6:::1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcpv6bracket
CLEANFILES+=		agent_tcp_tcpv6bracket.res agent_tcp_tcpv6bracket.exp
agent_tcp_tcpv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcpv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcpv6bracket_port
CLEANFILES+=		agent_tcp_tcpv6bracket_port.res agent_tcp_tcpv6bracket_port.exp
agent_tcp_tcpv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcpv6:[::1]:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPv6plain
CLEANFILES+=		agent_tcp_TCPv6plain.res agent_tcp_TCPv6plain.exp
agent_tcp_TCPv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCPv6:::1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPv6bracket
CLEANFILES+=		agent_tcp_TCPv6bracket.res agent_tcp_TCPv6bracket.exp
agent_tcp_TCPv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCPv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPv6bracket_port
CLEANFILES+=		agent_tcp_TCPv6bracket_port.res agent_tcp_TCPv6bracket_port.exp
agent_tcp_TCPv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCPv6:[::1]:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPv6bracket_port
CLEANFILES+=		agent_tcp_TCPv6bracket_port.res agent_tcp_TCPv6bracket_port.exp
agent_tcp_tcpipv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcpipv6:::1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcpipv6bracket
CLEANFILES+=		agent_tcp_tcpipv6bracket.res agent_tcp_tcpipv6bracket.exp
agent_tcp_tcpipv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcpipv6:[::1] sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_tcpipv6bracket_port
CLEANFILES+=		agent_tcp_tcpipv6bracket_port.res agent_tcp_tcpipv6bracket_port.exp
agent_tcp_tcpipv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic tcpipv6:[::1]:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPIPv6plain
CLEANFILES+=		agent_tcp_TCPIPv6plain.res agent_tcp_TCPIPv6plain.exp
agent_tcp_TCPIPv6plain:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCPIPv6:::1:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPIPv6bracket
CLEANFILES+=		agent_tcp_TCPIPv6bracket.res agent_tcp_TCPIPv6bracket.exp
agent_tcp_TCPIPv6bracket:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCPIPv6:[::1] sysServices.0 > $@.res
	diff -u $@.exp $@.res

REGRESS_TARGETS+=	agent_tcp_TCPIPv6bracket_port
CLEANFILES+=		agent_tcp_TCPIPv6bracket_port.res agent_tcp_TCPIPv6bracket_port.exp
agent_tcp_TCPIPv6bracket_port:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic TCPIPv6:[::1]:161 sysServices.0 > $@.res
	diff -up $@.exp $@.res

### USM

REGRESS_TARGETS+=	usm_noauthnopriv
CLEANFILES+=		usm_noauthnopriv.res usm_noauthnopriv.exp
usm_noauthnopriv:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l noauthnopriv -u md5_des 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authnopriv_md5
CLEANFILES+=		usm_authnopriv_md5.res usm_authnopriv_md5.exp
usm_authnopriv_md5:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authnopriv -u md5_des -a md5 -A testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authnopriv_sha1
CLEANFILES+=		usm_authnopriv_sha1.res usm_authnopriv_sha1.exp
usm_authnopriv_sha1:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authnopriv -u sha1_des -a sha -A testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authnopriv_sha224
CLEANFILES+=		usm_authnopriv_sha224.res usm_authnopriv_sha224.exp
usm_authnopriv_sha224:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authnopriv -u sha224_des -a sha-224 -A testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authnopriv_sha256
CLEANFILES+=		usm_authnopriv_sha256.res usm_authnopriv_sha256.exp
usm_authnopriv_sha256:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authnopriv -u sha256_des -a sha-256 -A testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authnopriv_sha384
CLEANFILES+=		usm_authnopriv_sha384.res usm_authnopriv_sha384.exp
usm_authnopriv_sha384:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authnopriv -u sha384_des -a sha-384 -A testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authnopriv_sha512
CLEANFILES+=		usm_authnopriv_sha512.res usm_authnopriv_sha512.exp
usm_authnopriv_sha512:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authnopriv -u sha512_des -a sha-512 -A testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_md5_des
CLEANFILES+=		usm_authpriv_md5_des.res usm_authpriv_md5_des.exp
usm_authpriv_md5_des:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u md5_des -a md5 -A testtest -x des -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_md5_aes
CLEANFILES+=		usm_authpriv_md5_aes.res usm_authpriv_md5_aes.exp
usm_authpriv_md5_aes:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u md5_aes -a md5 -A testtest -x aes -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha1_des
CLEANFILES+=		usm_authpriv_sha1_des.res usm_authpriv_sha1_des.exp
usm_authpriv_sha1_des:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha1_des -a sha -A testtest -x des -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha1_aes
CLEANFILES+=		usm_authpriv_sha1_aes.res usm_authpriv_sha1_aes.exp
usm_authpriv_sha1_aes:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha1_aes -a sha -A testtest -x aes -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha224_des
CLEANFILES+=		usm_authpriv_sha224_des.res usm_authpriv_sha224_des.exp
usm_authpriv_sha224_des:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha224_des -a sha-224 -A testtest -x des -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha224_aes
CLEANFILES+=		usm_authpriv_sha224_aes.res usm_authpriv_sha224_aes.exp
usm_authpriv_sha224_aes:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha224_aes -a sha-224 -A testtest -x aes -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha256_des
CLEANFILES+=		usm_authpriv_sha256_des.res usm_authpriv_sha256_des.exp
usm_authpriv_sha256_des:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha256_des -a sha-256 -A testtest -x des -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha256_aes
CLEANFILES+=		usm_authpriv_sha256_aes.res usm_authpriv_sha256_aes.exp
usm_authpriv_sha256_aes:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha256_aes -a sha-256 -A testtest -x aes -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha384_des
CLEANFILES+=		usm_authpriv_sha384_des.res usm_authpriv_sha384_des.exp
usm_authpriv_sha384_des:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha384_des -a sha-384 -A testtest -x des -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha384_aes
CLEANFILES+=		usm_authpriv_sha384_aes.res usm_authpriv_sha384_aes.exp
usm_authpriv_sha384_aes:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha384_aes -a sha-384 -A testtest -x aes -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha512_des
CLEANFILES+=		usm_authpriv_sha512_des.res usm_authpriv_sha512_des.exp
usm_authpriv_sha512_des:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha512_des -a sha-512 -A testtest -x des -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	usm_authpriv_sha512_aes
CLEANFILES+=		usm_authpriv_sha512_aes.res usm_authpriv_sha512_aes.exp
usm_authpriv_sha512_aes:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v3 -l authpriv -u sha512_aes -a sha-512 -A testtest -x aes -X testtest 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

### OUTPUT FORMAT

REGRESS_TARGETS+=	output_string
CLEANFILES+=		output_string.exp output_string.res
output_string:
	printf "sysDescr.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_integer
CLEANFILES+=		output_integer.res output_integer.exp
output_integer:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_oid
CLEANFILES+=		output_oid.res output_oid.exp
output_oid:
# The OID name used here is not compatible with net-snmp, so this
# might change in the future.
	printf "sysObjectID.0 = OID: localSystem.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1 sysObjectID.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_hexstring
CLEANFILES+=		output_hexstring.res output_hexstring.exp
.if ${IFIDX} == ""
REGRESS_SKIP_TARGETS+=	output_hexstring
.endif
output_hexstring:
# (Ab)use table for known unknown Hex-string. In this case: lladdr
	printf "ifPhysAddress.%d = Hex-STRING: %s\n" "${IFIDX}" "${IFLLADDR}" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1 ifPhysAddress.${IFIDX} > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_ipstring
CLEANFILES+=		output_ipstring.res output_ipstring.exp
output_ipstring:
	printf "ipAdEntAddr.127.0.0.1 = IpAddress: 127.0.0.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1 ipAdEntAddr.127.0.0.1 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiettype_string
CLEANFILES+=		output_quiettype_string.res output_quiettype_string.exp
output_quiettype_string:
	printf "sysDescr.0 = %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic -OQ 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiettype_integer
CLEANFILES+=		output_quiettype_integer.res output_quiettype_integer.exp
output_quiettype_integer:
	printf "sysServices.0 = 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic -OQ 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiettype_oid
CLEANFILES+=		output_quiettype_oid.res output_quiettype_oid.exp
output_quiettype_oid:
# The OID name used here is not compatible with net-snmp, so this
# might change in the future.
	printf "sysObjectID.0 = localSystem.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -OQ 127.0.0.1 sysObjectID.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiettype_ipstring
CLEANFILES+=		output_quiettype_ipstring.res output_quiettype_ipstring.exp
output_quiettype_ipstring:
	printf "ipAdEntAddr.127.0.0.1 = 127.0.0.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -OQ 127.0.0.1 ipAdEntAddr.127.0.0.1 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiettype_hexstring
CLEANFILES+=		output_quiettype_hexstring.res output_quiettype_hexstring.exp
.if ${IFIDX} == ""
REGRESS_SKIP_TARGETS+=	output_quiettype_hexstring
.endif
output_quiettype_hexstring:
# (Ab)use table for known unknown Hex-string. In this case: lladdr
	printf "ifPhysAddress.%d = %s\n" "${IFIDX}" "${IFLLADDR}" > $@.exp
	${SNMPGET} -v2c -cpublic -OQ 127.0.0.1 ifPhysAddress.${IFIDX} > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_varbind_string
CLEANFILES+=		output_varbind_string.res output_varbind_string.exp
output_varbind_string:
	printf "STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic -Ov 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_varbind_integer
CLEANFILES+=		output_varbind_integer.res output_varbind_integer.exp
output_varbind_integer:
	printf "INTEGER: 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Ov 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_varbind_oid
CLEANFILES+=		output_varbind_oid.res output_varbind_oid.exp
output_varbind_oid:
# The OID name used here is not compatible with net-snmp, so this
# might change in the future.
	printf "OID: localSystem.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Ov 127.0.0.1 sysObjectID.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_varbind_ipstring
CLEANFILES+=		output_varbind_ipstring.res output_varbind_ipstring.exp
output_varbind_ipstring:
	printf "IpAddress: 127.0.0.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Ov 127.0.0.1 ipAdEntAddr.127.0.0.1 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_varbind_hexstring
CLEANFILES+=		output_varbind_hexstring.res output_varbind_hexstring.exp
.if ${IFIDX} == ""
REGRESS_SKIP_TARGETS+=	output_varbind_hexstring
.endif
output_varbind_hexstring:
# (Ab)use table for known unknown Hex-string. In this case: lladdr
	printf "Hex-STRING: %s\n" "${IFLLADDR}" > $@.exp
	${SNMPGET} -v2c -cpublic -Ov 127.0.0.1 ifPhysAddress.${IFIDX} > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiettype_varbind
CLEANFILES+=		output_quiettype_varbind.res output_quiettype_varbind.exp
output_quiettype_varbind:
	uname -a > $@.exp
	${SNMPGET} -v2c -cpublic -OvQ 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_ascii
CLEANFILES+=		output_ascii.res output_ascii.exp
.if ${IFIDX} == ""
REGRESS_SKIP_TARGETS+=	output_ascii
.endif
output_ascii:
# Not a full test, since a mac-address can contain all kind of weirdness,
# so no guarantee that we handle everything. But I don't know a better
# alternative at this moment.
# Abuse $@.res for intermediate variable for easier cleanup.
	printf 'ifPhysAddress.%d = STRING: "' "${IFIDX}" > $@.exp
	printf "%s\n" "${IFLLADDR}" | tr ' ' '\n' | while read byte; do printf "\x$${byte}" | tr -c '[:print:]' '.'; done >> $@.exp
	printf '"\n' >> $@.exp
	${SNMPGET} -v2c -cpublic -Oa 127.0.0.1 ifPhysAddress.${IFIDX} > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_fulloid_mib
CLEANFILES+=		output_fulloid_mib.res output_fulloid_mib.exp
output_fulloid_mib:
	printf ".iso.org.dod.internet.mgmt.mib_2.system.sysDescr.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic -Of 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_fulloid_varbind
CLEANFILES+=		output_fulloid_varbind.res output_fulloid_varbind.exp
output_fulloid_varbind:
	printf ".iso.org.dod.internet.mgmt.mib_2.system.sysObjectID.0 = OID: .iso.org.dod.internet.private.enterprises.openBSD.localSystem.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Of 127.0.0.1 sysObjectID.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_numoid_mib
CLEANFILES+=		output_numoid_mib.res output_numoid_mib.exp
output_numoid_mib:
	printf ".1.3.6.1.2.1.1.1.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic -On 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_numoid_varbind
CLEANFILES+=		output_numoid_varbind.res output_numoid_varbind.exp
output_numoid_varbind:
	printf ".1.3.6.1.2.1.1.2.0 = OID: .1.3.6.1.4.1.30155.23.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -On 127.0.0.1 sysObjectID.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiet_string
CLEANFILES+=		output_quiet_string.res output_quiet_string.exp
output_quiet_string:
	printf "sysDescr.0 %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic -Oq 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiet_integer
CLEANFILES+=		output_quiet_integer.res output_quiet_integer.exp
output_quiet_integer:
	printf "sysServices.0 74\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Oq 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiet_oid
CLEANFILES+=		output_quiet_oid.res output_quiet_oid.exp
output_quiet_oid:
# The OID name used here is not compatible with net-snmp, so this
# might change in the future.
	printf "sysObjectID.0 localSystem.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Oq 127.0.0.1 sysObjectID.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiet_ipstring
CLEANFILES+=		output_quiet_ipstring.res output_quiet_ipstring.exp
output_quiet_ipstring:
	printf "ipAdEntAddr.127.0.0.1 127.0.0.1\n" > $@.exp
	${SNMPGET} -v2c -cpublic -Oq 127.0.0.1 ipAdEntAddr.127.0.0.1 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_quiet_hexstring
CLEANFILES+=		output_quiet_hexstring.res output_quiet_hexstring.exp
.if ${IFIDX} == ""
REGRESS_SKIP_TARGETS+=	output_quiet_hexstring
.endif
output_quiet_hexstring:
# (Ab)use table for known unknown Hex-string. In this case: lladdr
	printf "ifPhysAddress.%d %s\n" "${IFIDX}" "${IFLLADDR}" > $@.exp
	${SNMPGET} -v2c -cpublic -Oq 127.0.0.1 ifPhysAddress.${IFIDX} > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_hex
CLEANFILES+=		output_hex.res output_hex.exp
output_hex:
# From testing net-snmp: hex mode wraps each 16 bytes.
# It happens that od does the same.
	printf "localTest.3.2.0 = Hex-STRING: " > $@.exp
	printf "abcdefghijklmnopqrstuvwxyz" | od -An -tx1 | sed 's/^ *//;s/  / /g;s/ *$$//' | tr '[:lower:]' '[:upper:]' >> $@.exp
	${SNMPGET} -v2c -cpublic -Ox 127.0.0.1 1.3.6.1.4.1.30155.42.3.2.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255t_utf8
CLEANFILES+=		output_displayhint_255t_utf8.res output_displayhint_255t_utf8.exp
output_displayhint_255t_utf8:
	printf "STRING: Reyk Flöter\n" > $@.exp
	LC_ALL=en_US.UTF-8 ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.6.3.15.1.2.2.1.2.1.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255t_ascii
CLEANFILES+=		output_displayhint_255t_ascii.res output_displayhint_255t_ascii.exp
output_displayhint_255t_ascii:
	printf "STRING: Reyk Fl.ter\n" > $@.exp
	LC_ALL=C ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.6.3.15.1.2.2.1.2.1.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255t_invalchar_utf8
CLEANFILES+=		output_displayhint_255t_invalchar_utf8.res output_displayhint_255t_invalchar_utf8.exp
output_displayhint_255t_invalchar_utf8:
	printf "STRING: Reyk Fl�ter\n" > $@.exp
	LC_ALL=en_US.UTF-8 ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.6.3.15.1.2.2.1.2.2.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255t_invalchar_ascii
CLEANFILES+=		output_displayhint_255t_invalchar_ascii.res output_displayhint_255t_invalchar_ascii.exp
output_displayhint_255t_invalchar_ascii:
	printf "STRING: Reyk Fl?ter\n" > $@.exp
	LC_ALL=C ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.6.3.15.1.2.2.1.2.2.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255t_truncatefull
CLEANFILES+=		output_displayhint_255t_truncatefull.res output_displayhint_255t_truncatefull.exp
output_displayhint_255t_truncatefull:
	printf "STRING: %s\n" "$$(jot -ba 255 | tr -d '\n')" > $@.exp
	LC_ALL=en_US.UTF-8 ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.6.3.15.1.2.2.1.2.3.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255t_truncatehalf
CLEANFILES+=		output_displayhint_255t_truncatehalf.res output_displayhint_255t_truncatehalf.exp
output_displayhint_255t_truncatehalf:
	printf "STRING: %s\n" "$$(jot -ba 254 | tr -d '\n')" > $@.exp
	LC_ALL=en_US.UTF-8 ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.6.3.15.1.2.2.1.2.4.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255a_utf8
CLEANFILES+=		output_displayhint_255a_utf8.res output_displayhint_255a_utf8.exp
output_displayhint_255a_utf8:
	printf "STRING: Reyk Fl��ter\n" > $@.exp
	LC_ALL=en_US.UTF-8 ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.2.1.1.4.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_255a_ascii
CLEANFILES+=		output_displayhint_255a_ascii.res output_displayhint_255a_ascii.exp
output_displayhint_255a_ascii:
	printf "STRING: Reyk Fl??ter\n" > $@.exp
	LC_ALL=C-8 ${SNMPGET} -Ov -v2c -cpublic 127.0.0.1 1.3.6.1.2.1.1.4.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	output_displayhint_precedence
CLEANFILES+=		output_displayhint_precedence.res output_displayhint_precedence.exp
output_displayhint_precedence:
	printf "sysDescr.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -Ox -v2c -cpublic 127.0.0.1 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

### SNMP GET
# We already tested most of get in the previous tests.
# Just test the retrieval of multiple entries

REGRESS_TARGETS+=	get_multiple
CLEANFILES+=		get_multiple.res get_multiple.exp
get_multiple:
	printf "sysServices.0 = INTEGER: 74\nsysDescr.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGET} -v2c -cpublic 127.0.0.1 sysServices.0 sysDescr.0 > $@.res
	diff -up $@.exp $@.res

### SNMP GETNEXT

REGRESS_TARGETS+=	getnext_simple
CLEANFILES+=		getnext_simple.res getnext_simple.exp
getnext_simple:
	printf "sysDescr.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 system > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	getnext_multiple
CLEANFILES+=		getnext_multiple.res getnext_multiple.exp
getnext_multiple:
	printf "sysServices.0 = INTEGER: 74\nsysDescr.0 = STRING: %s\n" "$$(uname -a)" > $@.exp
	${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 sysLocation.0 system > $@.res
	diff -up $@.exp $@.res

### SNMP BULKGET
# Building output manually is large and error prone: Assume get/getnext works

REGRESS_TARGETS+=	bulkget_simple
CLEANFILES+=		bulkget_simple.res bulkget_simple.exp
bulkget_simple:
	IDX=1; while [ $$IDX -le 10 ]; do ${SNMPGET} -v2c -cpublic 127.0.0.1 sysORDescr.$$IDX; IDX=$$((IDX+1)); done > $@.exp
	${SNMPBULKGET} -v2c -cpublic 127.0.0.1 sysORDescr > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	bulkget_multi
CLEANFILES+=		bulkget_multi.res bulkget_multi.exp
bulkget_multi:
	> $@.exp
	IDX=1; while [ $$IDX -le 10 ]; do ${SNMPGET} -v2c -cpublic 127.0.0.1 sysORDescr.$$IDX sysORID.$$IDX; IDX=$$((IDX+1)); done >> $@.exp
	${SNMPBULKGET} -v2c -cpublic 127.0.0.1 sysORDescr sysORID > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	bulkget_nonrep
CLEANFILES+=		bulkget_nonrep.res bulkget_nonrep.exp
bulkget_nonrep:
	${SNMPGET} -v2c -cpublic 127.0.0.1 sysDescr.0 > $@.exp
	IDX=1; while [ $$IDX -le 10 ]; do ${SNMPGET} -v2c -cpublic 127.0.0.1 sysORDescr.$$IDX; IDX=$$((IDX+1)); done >> $@.exp
	${SNMPBULKGET} -v2c -cpublic -Cn1 127.0.0.1 sysDescr sysORDescr > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	bulkget_maxrep
CLEANFILES+=		bulkget_maxrep.res bulkget_maxrep.exp
bulkget_maxrep:
	IDX=1; while [ $$IDX -le 5 ]; do ${SNMPGET} -v2c -cpublic 127.0.0.1 sysORDescr.$$IDX; IDX=$$((IDX+1)); done > $@.exp
	${SNMPBULKGET} -v2c -cpublic -Cr5 127.0.0.1 sysORDescr > $@.res
	diff -up $@.exp $@.res

### SNMP WALK
# Building output manually is large and error prone: Assume getnext works
# Skip sysUptime, since it's a timer

REGRESS_TARGETS+=	walk_simple
CLEANFILES+=		walk_simple.res walk_simple.exp
walk_simple:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ -n "$${OID%sys*}" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	${SNMPWALK} -v2c -cpublic 127.0.0.1 system | grep -v ^sysUpTime.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	walk_end
CLEANFILES+=		walk_end.res walk_end.exp
walk_end:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ "$${OID%% *}" == "sysORLastChange.0" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	${SNMPWALK} -v2c -cpublic -CE sysORLastChange 127.0.0.1 system | grep -v ^sysUpTime.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	walk_includeoid
CLEANFILES+=		walk_includeoid.res walk_includeoid.exp
walk_includeoid:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPWALK} -v2c -cpublic 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	walk_excludeoid
CLEANFILES+=		walk_excludeoid.res walk_excludeoid.exp
walk_excludeoid:
	> $@.exp
	${SNMPWALK} -v2c -cpublic -CI 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	walk_summary
CLEANFILES+=		walk_summary.res walk_summary.exp
walk_summary:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ -n "$${OID%sys*}" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	wc -l $@.exp | awk '{printf("Variables found: %d\n", $$1 + 1)}' >> $@.exp
	${SNMPWALK} -v2c -cpublic -Cp 127.0.0.1 system | grep -v ^sysUpTime.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	walk_skip
CLEANFILES+=		walk_skip.exp walk_skip.res
walk_skip:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ -n "$${OID%sys*}" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	${SNMPWALK} -v2c -cpublic -Cs sysUpTime 127.0.0.1 system > $@.res
	diff -up $@.exp $@.res

### SNMP BULKWALK

REGRESS_TARGETS+=	bulkwalk_simple
CLEANFILES+=		bulkwalk_simple.res bulkwalk_simple.exp
bulkwalk_simple:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ -n "$${OID%sys*}" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	${SNMPBULKWALK} -v2c -cpublic 127.0.0.1 system | grep -v ^sysUpTime.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	bulkwalk_includeoid
CLEANFILES+=		bulkwalk_includeoid.res bulkwalk_includeoid.exp
bulkwalk_includeoid:
	printf "sysServices.0 = INTEGER: 74\n" > $@.exp
	${SNMPBULKWALK} -v2c -cpublic 127.0.0.1 sysServices.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	bulkwalk_summary
CLEANFILES+=		bulkwalk_summary.res bulkwalk_summary.exp
bulkwalk_summary:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ -n "$${OID%sys*}" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	wc -l $@.exp | awk '{printf("Variables found: %d\n", $$1 + 1)}' >> $@.exp
	${SNMPBULKWALK} -v2c -cpublic -Cp 127.0.0.1 system | grep -v ^sysUpTime.0 > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	bulkwalk_skip
CLEANFILES+=		bulkwalk_skip.exp bulkwalk_skip.res
bulkwalk_skip:
	OID=system; while true; do OID="$$(${SNMPGETNEXT} -v2c -cpublic 127.0.0.1 "$$OID")"; [ -n "$${OID%sys*}" ] && break; printf "%s\n" "$${OID}"; OID="$${OID%% *}"; done | grep -v ^sysUpTime.0 > $@.exp
	${SNMPBULKWALK} -v2c -cpublic -Cs sysUpTime 127.0.0.1 system > $@.res
	diff -up $@.exp $@.res

### SNMP SET

# XXX snmpd(8) doesn't support set
REGRESS_TARGETS+=	set_string
REGRESS_EXPECTED_FAILURES+= set_string
CLEANFILES+=		set_string.res set_string.exp
set_string:
	${SNMPGET} -v2c -cpublic -Oqv 127.0.0.1 1.3.6.1.4.1.30155.42.3.3.0 | awk '{ printf("%sa\n", $$1) }' > $@.exp
	${SNMPSET} -v2c -cprivate -Oqv 127.0.0.1 1.3.6.1.4.1.30155.42.3.3.0 s $$(cat $@.exp) > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	set_integer
REGRESS_EXPECTED_FAILURES+= set_integer
CLEANFILES+=		set_integer.res set_integer.exp
set_integer:
	${SNMPGET} -v2c -cpublic -Oqv 127.0.0.1 1.3.6.1.4.1.30155.42.3.4.0 | awk '{ print $$1 + 1 }' > $@.exp
	${SNMPSET} -v2c -cprivate -Oqv 127.0.0.1 1.3.6.1.4.1.30155.42.3.4.0 i $$(cat $@.exp) > $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	set_string_integer
REGRESS_EXPECTED_FAILURES+= set_string_integer
CLEANFILES+=		set_string_integer.res set_string_integer.exp
set_string_integer:
	${SNMPGET} -v2c -cpublic -Oqv 127.0.0.1 1.3.6.1.4.1.30155.42.3.3.0 1.3.6.1.4.1.30155.42.3.4.0 | \
	    awk 'NR == 1 { printf("%sa\n", $$1) } NR == 2 { print $$1 + 1 }' > $@.exp
	${SNMPSET} -v2c -cprivate -Oqv 127.0.0.1 1.3.6.1.4.1.30155.42.3.3.0 s $$(head -1 $@.exp) 1.3.6.1.4.1.30155.42.3.4.0 i $$(tail -1 $@.exp) > $@.res
	diff -up $@.exp $@.res

### SNMP TRAP

TRAP_EXEC!=		${SUDO} su -s /bin/sh _snmpd -c '[ -r ${.OBJDIR}/traphandle.sh ]' > /dev/null 2>&1 && printf 'yes' || printf 'no'

REGRESS_TARGETS+=	trap_simple
CLEANFILES+=		trap_simple.res trap_simple.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_simple
.endif
trap_simple:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_ip
CLEANFILES+=		trap_ip.res trap_ip.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_ip
.endif
trap_ip:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 192.168.1.1\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 a 192.168.1.1
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_bitstring
CLEANFILES+=		trap_bitstring.res trap_bitstring.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_bitstring
.endif
trap_bitstring:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 "a"\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 b '1,2,7'
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_counter32
CLEANFILES+=		trap_counter32.res trap_counter32.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_counter32
.endif
trap_counter32:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 20\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 c 20
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_decimal
CLEANFILES+=		trap_decimal.res trap_decimal.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_decimal
.endif
trap_decimal:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 "hello world"\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 d '104 101 108 108 111 32 119 111 114 108 100'
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_integer
CLEANFILES+=		trap_integer.res trap_integer.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_integer
.endif
trap_integer:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 42\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 i 42
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_null
CLEANFILES+=		trap_null.res trap_null.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_null
.endif
trap_null:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 n ignored
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_oid
CLEANFILES+=		trap_oid.res trap_oid.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_oid
.endif
trap_oid:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 iso.org.dod\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 o 1.3.6
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_string
CLEANFILES+=		trap_string.res trap_string.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_string
.endif
trap_string:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 "hello world"\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 s "hello world"
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_timeticks
CLEANFILES+=		trap_timeticks.res trap_timeticks.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_timeticks
.endif
trap_timeticks:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 420\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 t 420
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_unsigned
CLEANFILES+=		trap_unsigned.res trap_unsigned.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_unsigned
.endif
trap_unsigned:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 420\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 u 420
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_hex
CLEANFILES+=		trap_hex.res trap_hex.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_hex
.endif
trap_hex:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 "hello world"\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 x '68 65 6c 6c 6f 20 77 6f 72 6c 64'
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

REGRESS_TARGETS+=	trap_multi
CLEANFILES+=		trap_multi.res trap_multi.exp
.if ${TRAP_EXEC} != yes
REGRESS_SKIP_TARGETS+=	trap_multi
.endif
trap_multi:
	printf 'localhost\n127.0.0.1\niso.org.dod.internet.mgmt.mib_2.system.sysUpTime.0 100\n' > $@.exp
	printf 'iso.org.dod.internet.snmpV2.snmpModules.snmpMIB.snmpMIBObjects.snmpTrap.snmpTrapOID.0 iso.org.dod\n' >> $@.exp
	printf 'iso.org.dod.8 "hello world"\niso.org.dod.9 42\n' >> $@.exp
	> trap_output
	${SNMPTRAP} -v2c -cpublic 127.0.0.1 100 1.3.6 1.3.6.8 x '68 65 6c 6c 6f 20 77 6f 72 6c 64' 1.3.6.9 i 42
# Give snmpd some time to write out the data
	sleep 0.1
	cp trap_output $@.res
	diff -up $@.exp $@.res

.include <bsd.regress.mk>

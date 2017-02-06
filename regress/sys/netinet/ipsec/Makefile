#	$OpenBSD: Makefile,v 1.2 2017/02/06 22:58:51 bluhm Exp $

# This test needs a manual setup of four machines, the make
# target create-setup can be used distribute the configuration.
# The setup is the same as for regress/sys/net/pf_forward.
# Set up machines: SRC IPS RT ECO
# SRC is the machine where this makefile is running.
# IPS is running IPsec, it is reflecting or forwarding packets.
# RT is a router forwarding packets, maximum MTU is 1300.
# ECO is reflecting the ping and UDP and TCP echo packets.
#
# By choosing the net prefix of the outgoing packet the mode is selected
# 5 -> 5 : transport v4
# 4 -> 5 : transport v6
# 8 -> c : tunnel v4 stack v4
# 8 -> c : tunnel v4 stack v6
# 8 -> d : tunnel v6 stack v4
# 8 -> d : tunnel v6 stack v6
# 8 -> e : tunnel v4 forward v4
# 8 -> e : tunnel v4 forward v6
# 8 -> f : tunnel v6 forward v4
# 8 -> f : tunnel v6 forward v6
#
#               1400        1300
# +---+   0   +---+   1   +---+   2   +---+
# |SRC| ----> |IPS| ----> |RT | ----> |ECO|
# +---+ 458 5 +---+ cd    +---+    ef +---+
#     out    in   out    in   out    in
#

PREFIX_IPV4 ?=	10.188.1
PREFIX_IPV6 ?=	fdd7:e83e:66bc:1

# IPv4 outgoing address is selected by route if address of cloning route,
# so SRC_TRANSP_IPV4 and IPS_TRANSP_IPV4 must be in same net
# IPv6 outgoing address is selected common prefix, 4 and 5 are close together
# SRC_TRANSP_IPV6 and IPS_TRANSP_IPV6 should be in different network
# to avoid encryption of neighbor discovery packets

SRC_OUT_IPV4 ?=	${PREFIX_IPV4}00.17
SRC_OUT_IPV6 ?=	${PREFIX_IPV6}0::17
SRC_TRANSP_IPV4 ?=	${PREFIX_IPV4}05.17
SRC_TRANSP_IPV6 ?=	${PREFIX_IPV6}4::17
SRC_TUNNEL_IPV4 ?=	${PREFIX_IPV4}08.17
SRC_TUNNEL_IPV6 ?=	${PREFIX_IPV6}8::17

IPS_IN_IPV4 ?=	${PREFIX_IPV4}00.70
IPS_IN_IPV6 ?=	${PREFIX_IPV6}0::70
IPS_OUT_IPV4 ?=	${PREFIX_IPV4}01.70
IPS_OUT_IPV6 ?=	${PREFIX_IPV6}1::70
IPS_TRANSP_IPV4 ?=	${PREFIX_IPV4}05.70
IPS_TRANSP_IPV6 ?=	${PREFIX_IPV6}5::70
IPS_TUNNEL4_IPV4 ?=	${PREFIX_IPV4}12.70
IPS_TUNNEL4_IPV6 ?=	${PREFIX_IPV6}c::70
IPS_TUNNEL6_IPV4 ?=	${PREFIX_IPV4}13.70
IPS_TUNNEL6_IPV6 ?=	${PREFIX_IPV6}d::70

RT_IN_IPV4 ?=	${PREFIX_IPV4}01.71
RT_IN_IPV6 ?=	${PREFIX_IPV6}1::71
RT_OUT_IPV4 ?=	${PREFIX_IPV4}02.71
RT_OUT_IPV6 ?=	${PREFIX_IPV6}2::71

ECO_IN_IPV4 ?=	${PREFIX_IPV4}02.72
ECO_IN_IPV6 ?=	${PREFIX_IPV6}2::72
ECO_TUNNEL4_IPV4 ?=	${PREFIX_IPV4}14.72
ECO_TUNNEL4_IPV6 ?=	${PREFIX_IPV6}e::72
ECO_TUNNEL6_IPV4 ?=	${PREFIX_IPV4}15.72
ECO_TUNNEL6_IPV6 ?=	${PREFIX_IPV6}f::72

# Configure Addresses on the machines, there must be routes for the
# networks.  Adapt interface and addresse variables to your local
# setup.  To control the remote machine you need a hostname for
# ssh to log in.
#
# Run make create-setup to copy hostname.if files to the machines
# Run make check-setup to see if you got the setup correct.

SRC_OUT_IF ?=	tap4
IPS_IN_IF ?=	vio1
IPS_OUT_IF ?=	vio2
RT_IN_IF ?=	vio1
RT_OUT_IF ?=	vio2
ECO_IN_IF ?=	vio1

.if empty (IPS_SSH) || empty (RT_SSH) || empty (ECO_SSH)
regress:
	@echo this tests needs three remote machines to operate on
	@echo IPS_SSH RT_SSH ECO_SSH are empty
	@echo fill out these variables for additional tests, then
	@echo check wether your test machines are set up properly
	@echo SKIPPED
.endif

.MAIN: all

.if make (regress) || make (all)
.BEGIN: ipsec.conf addr.py
	@echo
	${SUDO} true
	ssh -t ${IPS_SSH} ${SUDO} true
	rm -f stamp-ipsec
.endif

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
.for host in SRC IPS RT ECO
.for dir in IN OUT
.for ipv in IF IPV4 IPV6
	echo '${host}_${dir}_${ipv}="${${host}_${dir}_${ipv}}"' >>$@.tmp
.endfor
.endfor
.endfor
.for host dir in SRC TRANSP SRC TUNNEL \
    IPS TRANSP IPS TUNNEL4 IPS TUNNEL6 \
    ECO TUNNEL4 ECO TUNNEL6
.for ipv in IPV4 IPV6
	echo '${host}_${dir}_${ipv}="${${host}_${dir}_${ipv}}"' >>$@.tmp
.endfor
.endfor
	mv $@.tmp $@

# load the ipsec sa and flow into the kernel of the SRC and IPS machine
stamp-ipsec: addr.py ipsec.conf
	@echo '\n======== $@ ========'
	${SUDO} ipsecctl -F
	cat addr.py ${.CURDIR}/ipsec.conf | ipsecctl -n -f -
	cat addr.py ${.CURDIR}/ipsec.conf | \
	    ${SUDO} ipsecctl -f -
	ssh ${IPS_SSH} ${SUDO} ipsecctl -F
	cat addr.py ${.CURDIR}/ipsec.conf | \
	    ssh ${IPS_SSH} ${SUDO} ipsecctl -f - \
	    -D FROM=to -D TO=from -D LOCAL=peer -D PEER=local
	@date >$@

# Ping all addresses.  This ensures that the IP addresses are configured
# and all routing table are set up to allow bidirectional packet flow.

run-regress-ping-IPS_TRANSP_IPV6:
	@echo '\n======== $@ ========'
	@echo 'IPv6 IPsec input does not filter enc0 interface with pf.  Echo'
	@echo 'request does not create state and echo reply does not pass pf.'
	@echo DISABLED


.for host dir in SRC OUT SRC TRANSP SRC TUNNEL \
    IPS IN IPS OUT IPS TRANSP IPS TUNNEL4 IPS TUNNEL6 \
    RT IN RT OUT \
    ECO IN ECO TUNNEL4 ECO TUNNEL6
.for ping ipv in ping IPV4 ping6 IPV6
TARGETS +=      ping-${host}_${dir}_${ipv}
run-regress-ping-${host}_${dir}_${ipv}:
	@echo '\n======== $@ ========'
	netstat -s -p esp | awk '/input ESP /{print $$1}' >esp.in
	netstat -s -p esp | awk '/output ESP /{print $$1}' >esp.out
	${ping} -n -c 1 -w 2 ${${host}_${dir}_${ipv}}
.if "${host}" != SRC && "${dir}" != IN && "${dir}" != OUT
	netstat -s -p esp | awk '/input ESP /{print $$1-1}' | diff esp.in -
	netstat -s -p esp | awk '/output ESP /{print $$1-1}' | diff esp.out -
.endif
.endfor
.endfor

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

${REGRESS_TARGETS}: stamp-ipsec

CLEANFILES +=		addr.py *.pyc *.log stamp-* */hostname.* *.{in,out}

.PHONY: create-setup

create-setup: stamp-hostname

etc/hostname.${SRC_OUT_IF}: Makefile
	@echo '\n======== $@ ========'
	mkdir -p ${@:H}
	rm -f $@ $@.tmp
	echo '### regress ipsec $@' >$@.tmp
.for dir in OUT TRANSP TUNNEL
	echo '# SRC_${dir}' >>$@.tmp
.for inet ipv masklen in inet IPV4 255.255.255.0 inet6 IPV6 64
	echo '${inet} alias ${SRC_${dir}_${ipv}} ${masklen}' >>$@.tmp
.endfor
.endfor
	echo '# IPS_TRANSP_IPV6/64 IPS_IN_IPV6' >>$@.tmp
	echo '!route -q delete -inet6 ${IPS_TRANSP_IPV6}/64' >>$@.tmp
	echo '!route add -inet6 ${IPS_TRANSP_IPV6}/64 ${IPS_IN_IPV6}' >>$@.tmp
.for host dir in RT IN ECO IN
	echo '# ${host}_${dir}/pfxlen IPS_IN' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${${host}_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${${host}_${dir}_${ipv}}/${pfxlen}'\
	    ${IPS_IN_${ipv}} >>$@.tmp
.endfor
.endfor
.for host in IPS ECO
.for dir in TUNNEL4 TUNNEL6
	echo '# ${host}_${dir}/pfxlen reject ${SRC_TUNNEL_${ipv}}' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${${host}_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${${host}_${dir}_${ipv}}/${pfxlen}'\
	    -reject ${SRC_TUNNEL_${ipv}} >>$@.tmp
.endfor
.endfor
.endfor
	mv $@.tmp $@

${IPS_SSH}/hostname.${IPS_IN_IF}: Makefile
	mkdir -p ${@:H}
	rm -f $@ $@.tmp
	echo '### regress ipsec $@' >$@.tmp
.for dir in IN TRANSP
	echo '# IPS_${dir}' >>$@.tmp
.for inet ipv masklen in inet IPV4 255.255.255.0 inet6 IPV6 64
	echo '${inet} alias ${IPS_${dir}_${ipv}} ${masklen}' >>$@.tmp
.endfor
.endfor
	echo '# SRC_TRANSP_IPV6/64 SRC_OUT_IPV6' >>$@.tmp
	echo '!route -q delete -inet6 ${SRC_TRANSP_IPV6}/64' >>$@.tmp
	echo '!route add -inet6 ${SRC_TRANSP_IPV6}/64 ${SRC_OUT_IPV6}' >>$@.tmp
.for host dir in SRC TUNNEL
	echo '# ${host}_${dir}/pfxlen reject ${IPS_IN_${ipv}}' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${${host}_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${${host}_${dir}_${ipv}}/${pfxlen}'\
	    -reject ${IPS_IN_${ipv}} >>$@.tmp
.endfor
.endfor
	mv $@.tmp $@

${IPS_SSH}/hostname.${IPS_OUT_IF}: Makefile
	@echo '\n======== $@ ========'
	mkdir -p ${@:H}
	rm -f $@ $@.tmp
	echo '### regress ipsec $@' >$@.tmp
.for dir in OUT TUNNEL4 TUNNEL6
	echo '# IPS_${dir}' >>$@.tmp
.for inet ipv masklen in inet IPV4 255.255.255.0 inet6 IPV6 64
	echo '${inet} alias ${IPS_${dir}_${ipv}} ${masklen}' >>$@.tmp
.endfor
.endfor
.for dir in IN TUNNEL4 TUNNEL6
	echo '# ECO_${dir}/pfxlen RT_IN' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${ECO_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${ECO_${dir}_${ipv}}/${pfxlen}'\
	    ${RT_IN_${ipv}} >>$@.tmp
.endfor
.endfor
	mv $@.tmp $@

${RT_SSH}/hostname.${RT_IN_IF}: Makefile
	@echo '\n======== $@ ========'
	mkdir -p ${@:H}
	rm -f $@ $@.tmp
	echo '### regress ipsec $@' >$@.tmp
	echo '# RT_IN' >>$@.tmp
.for inet ipv masklen in inet IPV4 255.255.255.0 inet6 IPV6 64
	echo '${inet} alias ${RT_IN_${ipv}} ${masklen}' >>$@.tmp
.endfor
.for dir in OUT TUNNEL
	echo '# SRC_${dir}/pfxlen IPS_OUT' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${SRC_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${SRC_${dir}_${ipv}}/${pfxlen}'\
	    ${IPS_OUT_${ipv}} >>$@.tmp
.endfor
.endfor
	mv $@.tmp $@

${RT_SSH}/hostname.${RT_OUT_IF}: Makefile
	@echo '\n======== $@ ========'
	mkdir -p ${@:H}
	rm -f $@ $@.tmp
	echo '### regress ipsec $@' >$@.tmp
	echo '# RT_OUT' >>$@.tmp
.for inet ipv masklen in inet IPV4 255.255.255.0 inet6 IPV6 64
	echo '${inet} alias ${RT_OUT_${ipv}} ${masklen}' >>$@.tmp
.endfor
.for dir in TUNNEL4 TUNNEL6
	echo '# ECO_${dir}/pfxlen ECO_IN' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${ECO_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${ECO_${dir}_${ipv}}/${pfxlen}'\
	    ${ECO_IN_${ipv}} >>$@.tmp
.endfor
.endfor
	mv $@.tmp $@

${ECO_SSH}/hostname.${ECO_IN_IF}: Makefile
	@echo '\n======== $@ ========'
	mkdir -p ${@:H}
	rm -f $@ $@.tmp
	echo '### regress ipsec $@' >$@.tmp
.for dir in IN TUNNEL4 TUNNEL6
	echo '# ECO_${dir}' >>$@.tmp
.for inet ipv masklen in inet IPV4 255.255.255.0 inet6 IPV6 64
	echo '${inet} alias ${ECO_${dir}_${ipv}} ${masklen}' >>$@.tmp
.endfor
.endfor
	echo '# IPS_OUT/pfxlen RT_OUT' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${IPS_OUT_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${IPS_OUT_${ipv}}/${pfxlen}'\
	    ${RT_OUT_${ipv}} >>$@.tmp
.endfor
.for dir in OUT TUNNEL
	echo '# SRC_${dir}/pfxlen RT_OUT' >>$@.tmp
.for inet ipv pfxlen in inet IPV4 24 inet6 IPV6 64
	echo '!route -q delete -${inet} ${SRC_${dir}_${ipv}}/${pfxlen}'\
	    >>$@.tmp
	echo '!route add -${inet} ${SRC_${dir}_${ipv}}/${pfxlen}'\
	    ${RT_OUT_${ipv}} >>$@.tmp
.endfor
.endfor
	mv $@.tmp $@

stamp-hostname: etc/hostname.${SRC_OUT_IF} \
    ${IPS_SSH}/hostname.${IPS_IN_IF} ${IPS_SSH}/hostname.${IPS_OUT_IF} \
    ${RT_SSH}/hostname.${RT_IN_IF} ${RT_SSH}/hostname.${RT_OUT_IF} \
    ${ECO_SSH}/hostname.${ECO_IN_IF}
	@echo '\n======== $@ ========'
	${SUDO} sh -c "umask 027;\
	    { sed '/^### regress/,\$$d' /etc/hostname.${SRC_OUT_IF} &&\
	    cat; } >/etc/hostname.${SRC_OUT_IF}.tmp"\
	    <etc/hostname.${SRC_OUT_IF}
	${SUDO} sh -c "mv /etc/hostname.${SRC_OUT_IF}.tmp\
	    /etc/hostname.${SRC_OUT_IF} &&\
	    sh /etc/netstart ${SRC_OUT_IF}"
.for host dir in IPS IN IPS OUT RT IN RT OUT ECO IN
	ssh ${${host}_SSH} ${SUDO} "umask 027;\
	    { sed '/^### regress/,\$$d' /etc/hostname.${${host}_${dir}_IF} &&\
	    cat; } >/etc/hostname.${${host}_${dir}_IF}.tmp"\
	    <${${host}_SSH}/hostname.${${host}_${dir}_IF}
	ssh ${${host}_SSH} ${SUDO} "mv /etc/hostname.${${host}_${dir}_IF}.tmp\
	    /etc/hostname.${${host}_${dir}_IF} &&\
	    sh /etc/netstart ${${host}_${dir}_IF}"
.endfor
	date >$@

.PHONY: check-setup

# Check wether the address, route and remote setup is correct
check-setup: check-setup-src check-setup-ips check-setup-rt check-setup-eco

check-setup-src:
	@echo '\n======== $@ ========'
.for ping inet ipv in ping inet IPV4 ping6 inet6 IPV6
.for host dir in SRC OUT SRC TRANSP SRC TUNNEL
	${ping} -n -c 1 ${${host}_${dir}_${ipv}}  # ${host}_${dir}_${ipv}
	route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    grep -q 'flags: .*LOCAL'  # ${host}_${dir}_${ipv}
.endfor
	${ping} -n -c 1 ${IPS_IN_${ipv}}  # IPS_IN_${ipv}
.for host dir in IPS OUT RT IN RT OUT ECO IN
	route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    fgrep -q 'gateway: ${IPS_IN_${ipv}}' \
	    # ${host}_${dir}_${ipv} IPS_IN_${ipv}
.endfor
.for host dir in IPS TUNNEL4 IPS TUNNEL6 ECO TUNNEL4 ECO TUNNEL6
	route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    grep -q 'flags: .*REJECT'  # ${host}_${dir}_${ipv}
.endfor
.endfor
	route -n get -inet ${IPS_TRANSP_IPV4} |\
	    egrep -q 'flags: .*(CLONING|CLONED)' # IPS_TRANSP_IPV4
	route -n get -inet6 ${IPS_TRANSP_IPV6} |\
	    fgrep -q 'gateway: ${IPS_IN_IPV6}' \
	    # IPS_TRANSP_IPV6 IPS_IN_IPV6

check-setup-ips:
	@echo '\n======== $@ ========'
.for ping inet ipv in ping inet IPV4 ping6 inet6 IPV6
.for host dir in IPS IN IPS OUT IPS TRANSP IPS TUNNEL4 IPS TUNNEL6
	ssh ${IPS_SSH} ${ping} -n -c 1 ${${host}_${dir}_${ipv}} \
	    # ${host}_${dir}_${ipv}
	ssh ${IPS_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    grep -q 'flags: .*LOCAL'  # ${host}_${dir}_${ipv}
.endfor
	ssh ${IPS_SSH} ${ping} -n -c 1 ${SRC_OUT_${ipv}}  # SRC_OUT_${ipv}
	ssh ${IPS_SSH} ${ping} -n -c 1 ${RT_IN_${ipv}}  # RT_IN_${ipv}
.for host dir in RT OUT ECO IN ECO TUNNEL4 ECO TUNNEL6
	ssh ${IPS_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    fgrep -q 'gateway: ${RT_IN_${ipv}}' \
	    # ${host}_${dir}_${ipv} RT_IN_${ipv}
.endfor
.for host dir in SRC TUNNEL
	ssh ${IPS_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    grep -q 'flags: .*REJECT'  # ${host}_${dir}_${ipv}
.endfor
.endfor
	ssh ${IPS_SSH} route -n get -inet ${SRC_TRANSP_IPV4} |\
	    egrep -q 'flags: .*(CLONING|CLONED)' # SRC_TRANSP_IPV4
	ssh ${IPS_SSH} route -n get -inet6 ${SRC_TRANSP_IPV6} |\
	    fgrep -q 'gateway: ${SRC_OUT_IPV6}' \
	    # SRC_TRANSP_IPV6 SRC_OUT_IPV6

check-setup-rt:
	@echo '\n======== $@ ========'
.for ping inet ipv in ping inet IPV4 ping6 inet6 IPV6
.for host dir in RT IN RT OUT
	ssh ${RT_SSH} ${ping} -n -c 1 ${${host}_${dir}_${ipv}} \
	    # ${host}_${dir}_${ipv}
	ssh ${RT_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    grep -q 'flags: .*LOCAL'  # ${host}_${dir}_${ipv}
.endfor
	ssh ${RT_SSH} ${ping} -n -c 1 ${IPS_OUT_${ipv}}  # IPS_OUT_${ipv}
.for host dir in IPS IN SRC OUT SRC TUNNEL
	ssh ${RT_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    fgrep -q 'gateway: ${IPS_OUT_${ipv}}' \
	    # ${host}_${dir}_${ipv} IPS_OUT_${ipv}
.endfor
	ssh ${RT_SSH} ${ping} -n -c 1 ${ECO_IN_${ipv}}  # ECO_IN_${ipv}
.for host dir in ECO TUNNEL4 ECO TUNNEL6
	ssh ${RT_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    fgrep -q 'gateway: ${ECO_IN_${ipv}}' \
	    # ${host}_${dir}_${ipv} ECO_IN_${ipv}
.endfor
.endfor

check-setup-eco:
	@echo '\n======== $@ ========'
.for ping inet ipv in ping inet IPV4 ping6 inet6 IPV6
.for host dir in ECO IN ECO TUNNEL4 ECO TUNNEL6
	ssh ${ECO_SSH} ${ping} -n -c 1 ${${host}_${dir}_${ipv}} \
	    # ${host}_${dir}_${ipv}
	ssh ${ECO_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    grep -q 'flags: .*LOCAL'  # ${host}_${dir}_${ipv}
.endfor
	ssh ${ECO_SSH} ${ping} -n -c 1 ${RT_OUT_${ipv}}  # RT_OUT_${ipv}
.for host dir in RT IN IPS OUT IPS IN SRC OUT SRC TUNNEL
	ssh ${ECO_SSH} route -n get -${inet} ${${host}_${dir}_${ipv}} |\
	    fgrep -q 'gateway: ${RT_OUT_${ipv}}' \
	    # ${host}_${dir}_${ipv} RT_OUT_${ipv}
.endfor
.endfor

.include <bsd.regress.mk>

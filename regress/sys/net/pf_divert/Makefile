#	$OpenBSD: Makefile,v 1.19 2017/12/18 17:01:27 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
'
.if ! empty(PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
	@echo SKIPPED
.endif

# Fill out these variables as you have to test divert with the pf
# kernel running on a remote machine.  You have to specify a local
# and remote ip address for the test connections.  The fake ip address
# will be routed via the remote address to test divert with non-existing
# addresses.  To control the remote machine you need a hostname for
# ssh to log in.  All the test files must be in the same directory
# local and remote.
# You must have an anchor "regress" for the divert rules in the pf.conf
# of the remote machine.  The kernel of the remote machine gets testet.
#
# Run make check-setup to see if you got the setup correct.

LOCAL_ADDR ?=
REMOTE_ADDR ?=
FAKE_ADDR ?=
LOCAL_ADDR6 ?=
REMOTE_ADDR6 ?=
FAKE_ADDR6 ?=
REMOTE_SSH ?=

.if empty (LOCAL_ADDR) || empty (REMOTE_ADDR) || empty (FAKE_ADDR) || \
    empty (LOCAL_ADDR6) || empty (REMOTE_ADDR6) || empty (FAKE_ADDR6) || \
    empty (REMOTE_SSH)
regress:
	@echo This tests needs a remote machine to operate on.
	@echo LOCAL_ADDR REMOTE_ADDR FAKE_ADDR LOCAL_ADDR6
	@echo REMOTE_ADDR6 FAKE_ADDR6 REMOTE_SSH are empty.
	@echo Fill out these variables for additional tests.
	@echo SKIPPED
.endif

# Automatically generate regress targets from test cases in directory.

PERLS =			Client.pm Packet.pm Proc.pm Remote.pm Server.pm \
			funcs.pl remote.pl
ARGS !=			cd ${.CURDIR} && ls args-*.pl
TARGETS ?=		\
	inet-args-tcp-to inet6-args-tcp-to \
	inet-args-tcp-reply inet6-args-tcp-reply \
	inet-args-udp-to inet6-args-udp-to \
	inet-args-udp-reply inet6-args-udp-reply \
	inet-args-udp-reply-to inet6-args-udp-reply-to \
	inet-args-rip-to inet6-args-rip-to \
	inet-args-rip-reply inet6-args-rip-reply \
	inet-args-rip-reply-to inet6-args-rip-reply-to \
	inet-args-icmp-to inet6-args-icmp-to \
	inet-args-icmp-reply-to inet6-args-icmp-reply-to \
	inet-args-icmp-reply-reuse inet6-args-icmp-reply-reuse \
	inet-reuse-tcp-to-to inet6-reuse-tcp-to-to \
	inet-reuse-tcp-to-reply inet6-reuse-tcp-to-reply \
	inet-reuse-tcp-reply-to inet6-reuse-tcp-reply-to \
	inet-reuse-tcp-reply-reply inet6-reuse-tcp-reply-reply \
	inet-reuse-udp-to-to inet6-reuse-udp-to-to \
	inet-reuse-udp-to-reply inet6-reuse-udp-to-reply \
	inet-reuse-udp-to-reply-to inet6-reuse-udp-to-reply-to \
	inet-reuse-udp-reply-to inet6-reuse-udp-reply-to \
	inet-reuse-udp-reply-reply inet6-reuse-udp-reply-reply \
	inet-reuse-udp-reply-reply-to inet6-reuse-udp-reply-reply-to \
	inet-reuse-udp-reply-to-to inet6-reuse-udp-reply-to-to \
	inet-reuse-udp-reply-to-reply inet6-reuse-udp-reply-to-reply \
	inet-reuse-udp-reply-to-reply-to inet6-reuse-udp-reply-to-reply-to \
	inet-reuse-rip-to-to inet6-reuse-rip-to-to \
	inet-reuse-rip-to-reply inet6-reuse-rip-to-reply \
	inet-reuse-rip-to-reply-to inet6-reuse-rip-to-reply-to \
	inet-reuse-rip-reply-to inet6-reuse-rip-reply-to \
	inet-reuse-rip-reply-reply inet6-reuse-rip-reply-reply \
	inet-reuse-rip-reply-reply-to inet6-reuse-rip-reply-reply-to \
	inet-reuse-rip-reply-to-to inet6-reuse-rip-reply-to-to \
	inet-reuse-rip-reply-to-reply inet6-reuse-rip-reply-to-reply \
	inet-reuse-rip-reply-to-reply-to inet6-reuse-rip-reply-to-reply-to \
	inet-args-udp-packet-in inet6-args-udp-packet-in \
	inet-args-udp-packet-out inet6-args-udp-packet-out
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
CLEANFILES +=		*.log *.port *.ktrace ktrace.out stamp-*

.MAIN: all

.if ! empty (REMOTE_SSH)
.if make (regress) || make (all)
.BEGIN:
	@echo
	${SUDO} true
	ssh -t ${REMOTE_SSH} ${SUDO} true
.if ! empty (FAKE_ADDR) && ! empty (REMOTE_ADDR)
	-${SUDO} route -n delete -inet -host ${FAKE_ADDR} 2>/dev/null
	${SUDO} route -n add -inet -host ${FAKE_ADDR} ${REMOTE_ADDR}
.endif
.if ! empty (FAKE_ADDR6) && ! empty (REMOTE_ADDR6)
	-${SUDO} route -n delete -inet6 -host ${FAKE_ADDR6} 2>/dev/null
	${SUDO} route -n add -inet6 -host ${FAKE_ADDR6} ${REMOTE_ADDR6}
.endif
.endif
.endif

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =	-I.
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# The arg tests take a perl hash with arguments controlling the test
# parameters.  The remote.pl test has local client or server and the
# diverted process is running on the remote machine reachable with
# ssh.

.for  inet addr  in  inet ADDR  inet6 ADDR6

run-regress-${inet}-reuse-rip-to-reply-to:
	@echo '\n======== $@ ========'
	@echo 'rip to before reply is broken, it does not remove the state.'
	@echo DISABLED

.for a in ${ARGS}
run-regress-${inet}-${a:R}: ${a}
	@echo '\n======== $@ ========'
.if ${@:M*-packet-*}
	time ${SUDO} SUDO=${SUDO} KTRACE=${KTRACE} \
	    perl ${PERLINC} ${PERLPATH}remote.pl -f ${inet} \
	    ${LOCAL_${addr}} ${REMOTE_${addr}} ${REMOTE_SSH} \
	    ${PERLPATH}${a}
.else
	time ${SUDO} SUDO=${SUDO} KTRACE=${KTRACE} \
	    perl ${PERLINC} ${PERLPATH}remote.pl -f ${inet} \
	    ${LOCAL_${addr}} ${FAKE_${addr}} ${REMOTE_SSH} \
	    ${PERLPATH}${a}
.endif
.endfor

.for proto in tcp udp rip

.for  first second  in  to to  to reply  to reply-to  reply to  reply reply  reply reply-to  reply-to to  reply-to reply  reply-to reply-to

run-regress-${inet}-reuse-${proto}-${first}-${second}:
	@echo '\n======== $@ ========'
	time ${SUDO} SUDO=${SUDO} KTRACE=${KTRACE} \
	    perl ${PERLINC} ${PERLPATH}remote.pl -f ${inet} \
	    ${LOCAL_${addr}} ${FAKE_${addr}} ${REMOTE_SSH} \
	    ${PERLPATH}args-${proto}-${first}.pl
	sed -n '/^connect peer:/s/.* //p' client.log >client.port
	sed -n '/^connect sock:/s/.* //p' client.log >server.port
.if "tcp" == ${proto}
.if "reply" == ${first}
	${SUDO} tcpdrop \
	    ${LOCAL_${addr}} `cat client.port` \
	    ${FAKE_${addr}} `cat server.port`
.endif
.if "to" == ${first}
	ssh ${REMOTE_SSH} ${SUDO} tcpdrop \
	    ${FAKE_${addr}} `cat client.port` \
	    ${LOCAL_${addr}} `cat server.port`
.endif
.endif
	time ${SUDO} SUDO=${SUDO} KTRACE=${KTRACE} \
	    perl ${PERLINC} ${PERLPATH}remote.pl ${inet} \
	    ${LOCAL_${addr}} ${FAKE_${addr}} ${REMOTE_SSH} \
	    `cat client.port` `cat server.port` \
	    ${PERLPATH}args-${proto}-${second}.pl
.if "tcp" == ${proto}
.if "reply" == ${second}
	${SUDO} tcpdrop \
	    ${LOCAL_${addr}} `cat server.port` \
	    ${FAKE_${addr}} `cat client.port`
.endif
.if "to" == ${second}
	ssh ${REMOTE_SSH} ${SUDO} pfctl -ss | \
	    egrep 'all ${proto} ${FAKE_${addr}}:?\[?'`cat server.port`\]?' .. ${LOCAL_${addr}}:?\[?'`cat client.port`'\]? '
	ssh ${REMOTE_SSH} ${SUDO} tcpdrop \
	    ${FAKE_${addr}} `cat server.port` \
	    ${LOCAL_${addr}} `cat client.port`
	ssh ${REMOTE_SSH} ${SUDO} pfctl -ss | \
	    ! egrep 'all ${proto} ${FAKE_${addr}}:?\[?'`cat server.port`\]?' .. ${LOCAL_${addr}}:?\[?'`cat client.port`'\]? '
.endif
.endif

.endfor
.endfor
.endfor

.PHONY: syntax check-setup

# make perl syntax check for all args files
syntax: stamp-syntax

stamp-syntax: ${PERLS} ${ARGS}
.for p in ${PERLS}
	@perl -c ${PERLINC} ${PERLPATH}$p
.endfor
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

# Check wether the address, route and remote setup is correct
check-setup:
	@echo '\n======== $@ ========'
	ping -n -c 1 ${LOCAL_ADDR}
	ping -n -c 1 ${REMOTE_ADDR}
	ping6 -n -c 1 ${LOCAL_ADDR6}
	ping6 -n -c 1 ${REMOTE_ADDR6}
	route -n get -inet ${FAKE_ADDR} | grep 'if address: ${LOCAL_ADDR}$$'
	route -n get -inet ${FAKE_ADDR} | grep 'gateway: ${REMOTE_ADDR}$$'
	route -n get -inet6 ${FAKE_ADDR6} | grep 'if address: ${LOCAL_ADDR6}$$'
	route -n get -inet6 ${FAKE_ADDR6} | grep 'gateway: ${REMOTE_ADDR6}$$'
	ssh ${REMOTE_SSH} ${SUDO} pfctl -sr | grep '^anchor "regress" all$$'
	ssh ${REMOTE_SSH} ${SUDO} pfctl -si | grep '^Status: Enabled '
	ssh ${REMOTE_SSH} perl -MIO::Socket::INET6 -MSocket6 -e 1

.include <bsd.regress.mk>

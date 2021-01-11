#	$OpenBSD: Makefile,v 1.1 2021/01/11 12:02:53 bluhm Exp $

# Copyright (c) 2021 Alexander Bluhm <bluhm@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Basic testing of the pflog(4) interface.  Create special routing
# doamin, load rules into pf(4) regress anchor, tcpdump on pflog, 
# send packets over lo(4), grep for expected result in tcpdump output.

# This test uses routing domain 11 and pflog interface number 11, 12, 13.
# Adjust it here, if you want to use something else.
N1 =		11
N2 =		12
N3 =		13
N =		${N1}
NUMS =		${N1} ${N2} ${N3}
IPS =		1 2 3 4 5 6

UID !!=		id -u

.include <bsd.own.mk>

.if ! (make(clean) || make(cleandir) || make(obj))

PF_STATUS !=	${SUDO} pfctl -si | sed -n 's/^Status: \([^ ]*\) .*/\1/p'
.if empty(PF_STATUS:MEnabled)
regress:
	@echo pf status: "${PF_STATUS}"
	@echo Enable pf to run this regress.
	@echo SKIPPED
.endif

PF_SKIP !=	${SUDO} pfctl -sI -v | sed -n 's/ (skip)//p'
.if ! empty(PF_SKIP:Mlo*:Nlo0)
regress:
	@echo pf skip: "${PF_SKIP}"
	@echo Do not set skip on interface lo or lo$N.
	@echo SKIPPED
.endif

PF_ANCHOR !=	${SUDO} pfctl -sr | sed -n 's/^anchor "\([^"]*\)" all$$/\1/p'
.if empty(PF_ANCHOR:Mregress)
regress:
	@echo pf anchor: "${PF_ANCHOR}"
	@echo Need anchor '"regress"' in pf.conf to load additional rules.
	@echo SKIPPED
.endif

.endif

.PHONY: busy-rdomains ifconfig unconfig pfctl

REGRESS_SETUP_ONCE +=	busy-rdomains
busy-rdomains:
	# Check if rdomains are busy.
	@if /sbin/ifconfig | grep -v '^lo$N:' | grep ' rdomain $N '; then\
	    echo routing domain $N is already used >&2; exit 1; fi

REGRESS_SETUP_ONCE +=	ifconfig
ifconfig: unconfig
	# Create and configure pflog and loopback interfaces.
.for n in ${NUMS}
	${SUDO} ifconfig pflog$n create
.endfor
	${SUDO} ifconfig lo$N rdomain $N
	${SUDO} ifconfig lo$N inet 127.0.0.1/8
.for i in ${IPS:N1}
	${SUDO} ifconfig lo$N inet 127.0.0.$i/32 alias
.endfor

REGRESS_CLEANUP +=	unconfig
unconfig: stamp-stop
	# Destroy interfaces.
.for i in ${IPS}
	-${SUDO} ifconfig lo$N inet 127.0.0.$i delete
.endfor
.for n in ${NUMS}
	-${SUDO} ifconfig pflog$n destroy
.endfor
	rm -f stamp-ifconfig

addr.py: Makefile
	# Create python include file containing the addresses.
	rm -f $@ $@.tmp
	echo 'N="$N"' >>$@.tmp
	echo 'LO="lo$N"' >>$@.tmp
.for var in N1 N2 N3
	echo '${var}="${${var}}"' >>$@.tmp
	echo 'PFLOG_${var}="pflog${${var}}"' >>$@.tmp
.endfor
	mv $@.tmp $@

REGRESS_SETUP_ONCE +=	pfctl
pfctl: addr.py pf.conf
	# Load the pf rules into the kernel.
	cat addr.py ${.CURDIR}/pf.conf | /sbin/pfctl -n -f -
	cat addr.py ${.CURDIR}/pf.conf | ${SUDO} pfctl -a regress -f -

# Run tcpdump on pflog devices.
DUMPCMD =	/usr/sbin/tcpdump -l -e -vvv -s 2048 -ni

stamp-bpf: stamp-bpf-${N1} stamp-bpf-${N2} stamp-bpf-${N3}
	sleep 2  # XXX
	@date >$@

.for n in ${NUMS}

stamp-bpf-$n: stamp-ifconfig
	rm -f pflog$n.tcpdump
	${SUDO} pkill -f '^${DUMPCMD} pflog$n' || true
	${SUDO} ${DUMPCMD} pflog$n >pflog$n.tcpdump &
	rm -f stamp-stop
	@date >$@

.endfor

stamp-stop:
	sleep 2  # XXX
	-${SUDO} pkill -f '^${DUMPCMD}'
	rm -f stamp-bpf*
	@date >$@

.for i in ${IPS}
REGRESS_TARGETS +=	run-ping-$i
run-ping-$i: stamp-bpf
	ping -n -w 1 -c 1 -V $N 127.0.0.$i

REGRESS_TARGETS +=	run-udp-$i
run-udp-$i: stamp-bpf
	echo foo | nc -u -V $N 127.0.0.$i discard
.endfor

.for n in ${NUMS}
REGRESS_TARGETS +=	run-bpf-$n
run-bpf-$n: stamp-stop
	cat pflog$n.tcpdump
.endfor

REGRESS_TARGETS +=	run-bpf-nothing
run-bpf-nothing: stamp-stop
	# rule with pflog${N3} is never used
	! grep . pflog${N3}.tcpdump

REGRESS_TARGETS +=	run-bpf-everything
run-bpf-everything: stamp-stop
	# rule with pflog${N2} matches on every packet
.for i in ${IPS}
	grep 'regress\.1/.* > 127.0.0.$i:' pflog${N2}.tcpdump
.endfor

REGRESS_TARGETS +=	run-bpf-all
run-bpf-all: stamp-stop
	# reply without keep state
	grep 'regress\.3/.* > 127.0.0.1: icmp: echo request' pflog${N1}.tcpdump
	grep 'regress\.3/.* > 127.0.0.1: icmp: echo reply' pflog${N1}.tcpdump
	# no reply with keep state and without all
	grep 'regress\.4/.* > 127.0.0.2: icmp: echo request' pflog${N1}.tcpdump
	! grep 'regress\.4/.* > 127.0.0.2: icmp: echo reply' pflog${N1}.tcpdump
	# reply with keep state and with all
	grep 'regress\.5/.* > 127.0.0.3: icmp: echo request' pflog${N1}.tcpdump
	# XXX anchor name missing
	grep '/.* > 127.0.0.3: icmp: echo reply' pflog${N1}.tcpdump

REGRESS_TARGETS +=	run-bpf-user
run-bpf-user: stamp-stop
	# out rule creates log entry with uid
	grep 'regress\.6/.* pass out on lo$N: \[uid ${UID}, pid [0-9]*\]\
	    127.* > 127.0.0.4.9:.* udp' pflog${N1}.tcpdump
	# in rule has no uid at log entry
	grep 'regress\.6/.* pass in on lo$N:\
	    127.* > 127.0.0.4.9:.* udp' pflog${N1}.tcpdump
	# icmp has no uid at log entry
	grep 'regress\.6/.* pass out on lo$N:\
	    127.* > 127.0.0.4: icmp: echo request' pflog${N1}.tcpdump
	# XXX rule without user sometimes has uid in log entry
	#grep 'regress\.3/.* pass out on lo$N:\
	    127.* > 127.0.0.1.9:.* udp' pflog${N1}.tcpdump
	@echo DISABLED

run-bpf-matches:
	# XXX The log matches keyword seems to be totally broken.
	# pf_log_matches() is never called.  Investigate later.
	@echo DISABLED

REGRESS_TARGETS +=	run-bpf-matches
run-bpf-matches: stamp-stop
	grep 'regress\.9/.* > 127.0.0.6: icmp: echo request' pflog${N1}.tcpdump
	! grep 'regress\.8/.* icmp: echo request' pflog${N1}.tcpdump
	! grep 'regress\.7/.* icmp: echo request' pflog${N1}.tcpdump

CLEANFILES +=	addr.py *.pyc *.tcpdump *.log stamp-*

.include <bsd.regress.mk>

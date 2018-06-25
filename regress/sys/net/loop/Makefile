#	$OpenBSD: Makefile,v 1.5 2018/06/25 22:22:49 bluhm Exp $

# Copyright (c) 2017 Alexander Bluhm <bluhm@openbsd.org>
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

# Set up two loopback interfaces in different routing domains.
# Try to ping existing and non existing addresses in these domains.
# Also test pinging to different rdomains via pf.  Check that the
# ttl is decremented while looping though loopback interfaces.

# This test uses routing doamin and interface number 11 and 12.
# Adjust it here, if you want to use something else.
N1 =		11
N2 =		12
NUMS =		${N1} ${N2}

.include <bsd.own.mk>

.if ! (make(clean) || make(cleandir) || make(obj))
SYSCTL_FORWARDING !=	sysctl net.inet.ip.forwarding

.if ${SYSCTL_FORWARDING:C/.*=//} != 1
regress:
	@echo "${SYSCTL_FORWARDING}"
	@echo set this sysctl to 1 for additional tests
	@echo SKIPPED
.endif

PF_SKIP_IFACE !=	${SUDO} /sbin/pfctl -sI -v | sed -n 's/ (skip)//p'

.if ! empty(PF_SKIP_IFACE:Mlo*:Nlo0)
regress:
	@echo "${PF_SKIP_IFACE}"
	@echo Do not set skip on interface lo, lo${N1}, or lo${N2}.
	@echo SKIPPED
.endif
.endif

.PHONY: busy-rdomains ifconfig unconfig

busy-rdomains:
	# check if rdomains are busy
.for n in ${NUMS}
	@if /sbin/ifconfig | grep -v '^lo${n}:' | grep ' rdomain ${n} '; then\
	    echo routing domain ${n} is already used >&2; exit 1; fi
.endfor

ifconfig: unconfig
	# create and configure loopback interfaces
.for n in ${NUMS}
	${SUDO} /sbin/ifconfig lo${n} rdomain ${n}
	${SUDO} /sbin/ifconfig lo${n} inet 127.0.0.1/8
	${SUDO} /sbin/ifconfig lo${n} inet 127.0.0.${n} alias
	${SUDO} /sbin/route -n -T ${n} add -inet -host 10.6.6.6 127.0.0.1
	${SUDO} /sbin/route -n -T ${n} add -inet -host 10.7.7.7 127.0.0.1
.endfor
	${SUDO} /sbin/route -n -T ${N1} add -inet -host 127.0.0.${N2} 127.0.0.1
	${SUDO} /sbin/route -n -T ${N2} add -inet -host 127.0.0.${N1} 127.0.0.1

unconfig:
	# destroy interfaces
.for n in ${NUMS}
	-${SUDO} /sbin/ifconfig lo${n} 127.0.0.1 delete
	-${SUDO} /sbin/ifconfig lo${n} 127.0.0.${n} delete
.endfor
	rm -f stamp-setup

stamp-setup: Makefile
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} busy-rdomains ifconfig
	date >$@

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
.for var in N1 N2
	echo '${var}="${${var}}"' >>$@.tmp
	echo 'IF_${var}="lo${${var}}"' >>$@.tmp
	echo 'ADDR_${var}="127.0.0.${${var}}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# Load the pf rules into the kernel.
stamp-pfctl: addr.py pf.conf stamp-setup
	@echo '\n======== $@ ========'
	cat addr.py ${.CURDIR}/pf.conf | /sbin/pfctl -n -f -
	cat addr.py ${.CURDIR}/pf.conf | ${SUDO} /sbin/pfctl -a regress -f -
	@date >$@

# run tcpdump on lo devices
DUMPCMD =	/usr/sbin/tcpdump -l -e -vvv -s 2048 -ni

stamp-bpf: stamp-bpf-${N1} stamp-bpf-${N2}
	sleep 2  # XXX
	@date >$@

.for n in ${N1} ${N2}

stamp-bpf-${n}: stamp-setup
	@echo '\n======== $@ ========'
	rm -f lo${n}.tcpdump
	${SUDO} pkill -f '^${DUMPCMD} lo${n}' || true
	${SUDO} ${DUMPCMD} lo${n} >lo${n}.tcpdump &
	rm -f stamp-stop
	@date >$@

.endfor

stamp-stop:
	@echo '\n======== $@ ========'
	sleep 2  # XXX
	-${SUDO} pkill -f '^${DUMPCMD}'
	rm -f stamp-bpf*
	@date >$@

.for n in ${N1} ${N2}

REGRESS_TARGETS +=	run-regress-ping-local-${n}
run-regress-ping-local-${n}: stamp-setup stamp-bpf
	@echo '\n======== $@ ========'
	# Ping localhost in routing domain ${n}.
	/sbin/ping -n -w 1 -c 1 -V ${n} 127.0.0.1

REGRESS_TARGETS +=	run-regress-ping-loop-${n}
run-regress-ping-loop-${n}: stamp-setup stamp-bpf
	@echo '\n======== $@ ========'
	# Ping non existing address with loopback route in routing domain ${n}.
	! /sbin/ping -n -w 1 -c 1 -V ${n} 10.6.6.6

REGRESS_TARGETS +=	run-regress-ping-address-${n}
run-regress-ping-address-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping local address in routing domain ${n}.
	/sbin/ping -n -w 1 -c 1 -V ${n} 127.0.0.${n}

.endfor

REGRESS_TARGETS +=	run-regress-ping-rdomain-pass
run-regress-ping-rdomain-pass: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Pass ping packets between routing domains with pf rule.
	/sbin/ping -n -w 1 -c 1 -V ${N1} 127.0.0.${N2}

REGRESS_TARGETS +=	run-regress-ping-rdomain-block
run-regress-ping-rdomain-block: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Check that reverse direction without pf rule is not allowed.
	! /sbin/ping -n -w 1 -c 1 -V ${N2} 127.0.0.${N1}

REGRESS_TARGETS +=	run-regress-ping-rdomain-loop
run-regress-ping-rdomain-loop: stamp-setup stamp-pfctl stamp-bpf
	@echo '\n======== $@ ========'
	# Ping non existing address and loop between routing domains.
	! /sbin/ping -n -w 1 -c 1 -V ${N1} 10.7.7.7

.for n in ${N1} ${N2}

REGRESS_TARGETS +=	run-regress-bpf-local-${n}
run-regress-bpf-local-${n}: stamp-stop
	@echo '\n======== $@ ========'
	# Check that the ping packet went through loopback.
	grep '127.0.0.1 > 127.0.0.1: icmp: echo request' lo${n}.tcpdump

REGRESS_TARGETS +=	run-regress-bpf-loop-${n}
run-regress-bpf-loop-${n}: stamp-stop
	@echo '\n======== $@ ========'
	# Check that the ping packet went multiple times through loopback.
	grep '[0-9] 127.0.0.1 > 10.6.6.6: icmp: echo request .*ttl 255,' \
	    lo${n}.tcpdump
	grep '[0-9] 127.0.0.1 > 10.6.6.6: icmp: echo request .* \[ttl 1\]' \
	    lo${n}.tcpdump

.endfor

REGRESS_TARGETS +=	run-regress-bpf-rdomain-loop-${N1}
run-regress-bpf-rdomain-loop-${N1}: stamp-stop
	@echo '\n======== $@ ========'
	# Check the ping packet went multiple times in routing domains.
	grep '[0-9] 127.0.0.1 > 10.7.7.7: icmp: echo request .*ttl 255,' \
	    lo${N1}.tcpdump
	! grep '[0-9] 127.0.0.1 > 10.7.7.7: icmp: echo request .*ttl 254,' \
	    lo${N1}.tcpdump
	grep '[0-9] 127.0.0.1 > 10.7.7.7: icmp: echo request .* \[ttl 1\]' \
	    lo${N1}.tcpdump

REGRESS_TARGETS +=	run-regress-bpf-rdomain-loop-${N2}
run-regress-bpf-rdomain-loop-${N2}: stamp-stop
	@echo '\n======== $@ ========'
	# Check the ping packet went multiple times in routing domains.
	grep '[0-9] 127.0.0.1 > 10.7.7.7: icmp: echo request .*ttl 254,' \
	    lo${N2}.tcpdump
	grep '[0-9] 127.0.0.1 > 10.7.7.7: icmp: echo request .*ttl 2,' \
	    lo${N2}.tcpdump
	! grep '[0-9] 127.0.0.1 > 10.7.7.7: icmp: echo request .* \[ttl 1\]' \
	    lo${N2}.tcpdump

REGRESS_TARGETS +=	run-regress-cleanup
run-regress-cleanup: stamp-stop
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

CLEANFILES +=	addr.py *.pyc *.tcpdump *.log stamp-*

.include <bsd.regress.mk>

#	$OpenBSD: Makefile,v 1.1.1.1 2017/11/17 10:26:58 bluhm Exp $

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
# Also test pinging to different rdomains via pf.

SYSCTL_FORWARDING !=       sysctl net.inet.ip.forwarding

.if ${SYSCTL_FORWARDING:C/.*=//} != 1
regress:
	@echo "${SYSCTL_FORWARDING}"
	@echo set this sysctl to 1 for additional tests
	@echo SKIPPED
.endif

# This test uses routing doamin and interface number 11 and 12.
# Adjust it here, if ue want to use something else.
N1 =		11
N2 =		12
NUMS =		${N1} ${N2}

.PHONY: check-interfaces check-rdomains ifconfig unconfig

check-rdomains:
	# check if rdomains are busy
.for n in ${NUMS}
	@if /sbin/ifconfig | grep -v '^lo${n}:' | grep ' rdomain ${n} '; then\
	    echo routing domain ${n} is already used >&2; exit 1; fi
.endfor

ifconfig: check-rdomains
	# create and configure loopback interfaces
.for n in ${NUMS}
	${SUDO} /sbin/ifconfig lo${n} rdomain ${n}
	${SUDO} /sbin/ifconfig lo${n} inet 127.0.0.1/8
	${SUDO} /sbin/ifconfig lo${n} inet 127.0.0.${n} alias
	${SUDO} /sbin/route -n -T ${n} add -inet -host 127.6.6.6 127.0.0.1
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
	${.MAKE} -C ${.CURDIR} ifconfig
	date >$@

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
.for var in N1 N2
	echo '${var}="${${var}}"' >>$@.tmp
	echo 'ADDR_${var}="127.0.0.${${var}}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# Load the pf rules into the kernel.
stamp-pfctl: addr.py pf.conf stamp-setup
	@echo '\n======== $@ ========'
	cat addr.py ${.CURDIR}/pf.conf | pfctl -n -f -
	cat addr.py ${.CURDIR}/pf.conf | ${SUDO} pfctl -a regress -f -
	@date >$@

.for n in ${N1} ${N2}

REGRESS_TARGETS +=	run-regress-ping-local-${n}
run-regress-ping-local-${n}: stamp-setup
	@echo '\n======== $@ ========'
	# Ping localhost in routing domain ${n}.
	/sbin/ping -n -w 1 -c 1 -V ${n} 127.0.0.1

REGRESS_TARGETS +=	run-regress-ping-loop-${n}
run-regress-ping-loop-${n}: stamp-setup
	@echo '\n======== $@ ========'
	# Ping non existing address with loopback route in routing domain ${n}.
	! /sbin/ping -n -w 1 -c 1 -V ${n} 127.6.6.6

REGRESS_TARGETS +=	run-regress-ping-address-${n}
run-regress-ping-address-${n}: stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping local address in routing domain ${n}.
	/sbin/ping -n -w 1 -c 1 -V ${n} 127.0.0.${n}

.endfor

REGRESS_TARGETS +=	run-regress-ping-rdomain-pass
run-regress-ping-rdomain-pass: stamp-pfctl
	@echo '\n======== $@ ========'
	# Pass ping packets between routing domains with pf rule.
	/sbin/ping -n -w 1 -c 1 -V ${N1} 127.0.0.${N2}

REGRESS_TARGETS +=	run-regress-ping-rdomain-block
run-regress-ping-rdomain-block: stamp-pfctl
	@echo '\n======== $@ ========'
	# Check that reverse direction without pf rule is not allowed.
	! /sbin/ping -n -w 1 -c 1 -V ${N2} 127.0.0.${N1}

REGRESS_TARGETS +=	run-regress-cleanup
run-regress-cleanup: stamp-setup
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

CLEANFILES +=	addr.py *.pyc *.log stamp-*

.include <bsd.regress.mk>

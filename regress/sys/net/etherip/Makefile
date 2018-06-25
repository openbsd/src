#	$OpenBSD: Makefile,v 1.6 2018/06/25 22:22:49 bluhm Exp $

# Copyright (c) 2017 Alexander Bluhm <bluhm@openbsd.org>
# Copyright (c) 2015 Vincent Gross <vgross@openbsd.org>
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

# This tests creates a routing domain setup with bridge, vether,
# etherip, pair.  Then ping packets are sent through these interfaces.
# Finally the setup is destroyed.

SYSCTL_ETHERIP !=	sysctl net.inet.etherip.allow

.if ${SYSCTL_ETHERIP:C/.*=//} != 1
regress:
	@echo "${SYSCTL_ETHERIP}"
	@echo set this sysctl to 1 for additional tests
	@echo SKIPPED
.endif

# This test uses routing domain and interface number 11 and 12.
# Adjust it here, if you want to use something else.
N1 =		11
N2 =		12
NUMS =		${N1} ${N2}
IFS =		bridge vether etherip pair

CLEANFILES =	stamp-*

.PHONY: busy-interfaces busy-rdomains ifconfig unconfig

busy-interfaces:
	# check if interfaces are busy
.for n in ${NUMS}
.for i in ${IFS}
	@if ! /sbin/ifconfig ${i}${n} 2>&1 | grep -q "no such interface"; then\
	    echo interface ${i}${n} is already used >&2; exit 1; fi
.endfor
.endfor

busy-rdomains:
	# check if rdomains are busy
.for n in ${NUMS}
	@if /sbin/ifconfig | grep -v '^lo${n}:' | grep ' rdomain ${n} '; then\
	    echo routing domain ${n} is already used >&2; exit 1; fi
.endfor

ifconfig: unconfig
	# create and configure interfaces
.for n in ${NUMS}
.for i in ${IFS}
	${SUDO} /sbin/ifconfig ${i}${n} create rdomain ${n}
.endfor
.endfor
	${SUDO} /sbin/ifconfig pair${N1}    172.31.0.1/24
	${SUDO} /sbin/ifconfig pair${N2}    172.31.0.2/24 patch pair${N1}
	${SUDO} /sbin/ifconfig vether${N1}  192.168.0.1
	${SUDO} /sbin/ifconfig vether${N2}  192.168.0.2
	${SUDO} /sbin/ifconfig etherip${N1} tunneldomain ${N1}
	${SUDO} /sbin/ifconfig etherip${N2} tunneldomain ${N2}
	${SUDO} /sbin/ifconfig bridge${N1}  add vether${N1} add etherip${N1} up
	${SUDO} /sbin/ifconfig bridge${N2}  add vether${N2} add etherip${N2} up
	${SUDO} /sbin/ifconfig etherip${N1} tunnel 172.31.0.1 172.31.0.2 up
	${SUDO} /sbin/ifconfig etherip${N2} tunnel 172.31.0.2 172.31.0.1 up

unconfig:
	# destroy interfaces
.for n in ${NUMS}
.for i in ${IFS}
	-${SUDO} /sbin/ifconfig ${i}${n} destroy
.endfor
.endfor
	rm -f stamp-setup

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} busy-interfaces busy-rdomains ifconfig
	date >$@

REGRESS_TARGETS +=	run-regress-ping-1-2
run-regress-ping-1-2: stamp-setup
	@echo '\n======== $@ ========'
	/sbin/ping -n -w 1 -c 1 -V ${N1} 192.168.0.2

REGRESS_TARGETS +=	run-regress-ping-2-1
run-regress-ping-2-1: stamp-setup
	@echo '\n======== $@ ========'
	/sbin/ping -n -w 1 -c 1 -V ${N2} 192.168.0.1

REGRESS_TARGETS +=	run-regress-cleanup
run-regress-cleanup: stamp-setup
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>

#	$OpenBSD: Makefile,v 1.4 2020/01/08 21:28:00 bluhm Exp $

# Copyright (c) 2018-2020 Alexander Bluhm <bluhm@openbsd.org>
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

# Set up two gif interfaces in different routing domains.
# Try to ping existing addresses in these domains with various tunnels.

# This test uses routing doamin and interface number 11 and 12.
# Adjust it here, if you want to use something else.
N1 =		11
N2 =		12
NUMS =		${N1} ${N2}

TUNNEL4_SRC_ADDR4_${N1} =	10.188.44.${N1}
TUNNEL4_DST_ADDR4_${N1} =	10.188.44.${N2}
TUNNEL6_SRC_ADDR4_${N1} =	10.188.64.${N1}
TUNNEL6_DST_ADDR4_${N1} =	10.188.64.${N2}
SRC_TUNNEL4_${N1} =		10.188.4.${N1}
DST_TUNNEL4_${N1} =		10.188.4.${N2}
TUNNEL4_SRC_ADDR6_${N1} =	fdd7:e83e:66bc:46::${N1}
TUNNEL4_DST_ADDR6_${N1} =	fdd7:e83e:66bc:46::${N2}
TUNNEL6_SRC_ADDR6_${N1} =	fdd7:e83e:66bc:66::${N1}
TUNNEL6_DST_ADDR6_${N1} =	fdd7:e83e:66bc:66::${N2}
SRC_TUNNEL6_${N1} =		fdd7:e83e:66bc:6::${N1}
DST_TUNNEL6_${N1} =		fdd7:e83e:66bc:6::${N2}

TUNNEL4_SRC_ADDR4_${N2} =	${TUNNEL4_DST_ADDR4_${N1}}
TUNNEL4_DST_ADDR4_${N2} =	${TUNNEL4_SRC_ADDR4_${N1}}
TUNNEL6_SRC_ADDR4_${N2} =	${TUNNEL6_DST_ADDR4_${N1}}
TUNNEL6_DST_ADDR4_${N2} =	${TUNNEL6_SRC_ADDR4_${N1}}
SRC_TUNNEL4_${N2} =		${DST_TUNNEL4_${N1}}
DST_TUNNEL4_${N2} =		${SRC_TUNNEL4_${N1}}
TUNNEL4_SRC_ADDR6_${N2} =	${TUNNEL4_DST_ADDR6_${N1}}
TUNNEL4_DST_ADDR6_${N2} =	${TUNNEL4_SRC_ADDR6_${N1}}
TUNNEL6_SRC_ADDR6_${N2} =	${TUNNEL6_DST_ADDR6_${N1}}
TUNNEL6_DST_ADDR6_${N2} =	${TUNNEL6_SRC_ADDR6_${N1}}
SRC_TUNNEL6_${N2} =		${DST_TUNNEL6_${N1}}
DST_TUNNEL6_${N2} =		${SRC_TUNNEL6_${N1}}

.include <bsd.own.mk>

.if ! (make(clean) || make(cleandir) || make(obj))

SYSCTL_FORWARDING !=	sysctl net.inet.ip.forwarding
.if ${SYSCTL_FORWARDING:C/.*=//} != 1
regress:
	@echo sysctl: "${SYSCTL_FORWARDING}"
	@echo Set sysctl to 1 to run this regress.
	@echo SKIPPED
.endif

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
	@echo Do not set skip on interface lo, lo${N1}, or lo${N2}.
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
	${SUDO} ifconfig lo${n} rdomain ${n}
	${SUDO} ifconfig lo${n} inet 127.0.0.1/8
	${SUDO} ifconfig lo${n} inet6 ::1/128
	${SUDO} ifconfig lo${n} inet ${SRC_TUNNEL4_${n}} alias
	${SUDO} ifconfig lo${n} inet6 ${SRC_TUNNEL6_${n}} alias
	${SUDO} route -n -T ${n} add -inet -host ${DST_TUNNEL4_${n}} 127.0.0.1
	${SUDO} route -n -T ${n} add -inet6 -host ${DST_TUNNEL6_${n}} ::1
	${SUDO} ifconfig gif${n}4 create rdomain ${n} tunneldomain ${n}
	${SUDO} ifconfig gif${n}4 tunnel ${SRC_TUNNEL4_${n}} ${DST_TUNNEL4_${n}}
	${SUDO} ifconfig gif${n}4 inet ${TUNNEL4_SRC_ADDR4_${n}}/32\
	    ${TUNNEL4_DST_ADDR4_${n}}
	${SUDO} ifconfig gif${n}4 inet6 ${TUNNEL4_SRC_ADDR6_${n}}\
	    ${TUNNEL4_DST_ADDR6_${n}}
	${SUDO} ifconfig gif${n}6 create rdomain ${n} tunneldomain ${n}
	${SUDO} ifconfig gif${n}6 tunnel ${SRC_TUNNEL6_${n}}\
	    ${DST_TUNNEL6_${n}}
	${SUDO} ifconfig gif${n}6 inet ${TUNNEL6_SRC_ADDR4_${n}}/32\
	    ${TUNNEL6_DST_ADDR4_${n}}
	${SUDO} ifconfig gif${n}6 inet6 ${TUNNEL6_SRC_ADDR6_${n}}\
	    ${TUNNEL6_DST_ADDR6_${n}}
.endfor

unconfig:
	# destroy interfaces, keep lo as routing domain cannot be destroyed
.for n in ${NUMS}
	-${SUDO} ifconfig lo${n} rdomain ${n}
	-${SUDO} ifconfig lo${n} inet 127.0.0.1 delete
	-${SUDO} ifconfig lo${n} inet6 ::1 delete
	-${SUDO} ifconfig lo${n} inet ${SRC_TUNNEL4_${n}} delete
	-${SUDO} ifconfig lo${n} inet6 ${SRC_TUNNEL6_${n}} delete
	-${SUDO} ifconfig gif${n}4 destroy
	-${SUDO} ifconfig gif${n}6 destroy
.endfor
	rm -f stamp-setup

stamp-setup: Makefile
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} busy-rdomains ifconfig
	# wait until DAD for inet6 tunnel addresses has finished
	sleep 1
	date >$@

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
.for N in N1 N2
	echo '${N}="${${N}}"' >>$@.tmp
	echo 'IF_${N}="lo${${N}}"' >>$@.tmp
	echo 'DST_TUNNEL4_${N}="${DST_TUNNEL4_${${N}}}"' >>$@.tmp
	echo 'DST_TUNNEL6_${N}="${DST_TUNNEL6_${${N}}}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# Load the pf rules into the kernel.
stamp-pfctl: addr.py pf.conf stamp-setup
	@echo '\n======== $@ ========'
	cat addr.py ${.CURDIR}/pf.conf | /sbin/pfctl -n -f -
	cat addr.py ${.CURDIR}/pf.conf | ${SUDO} pfctl -a regress -f -
	# ifconfig gif inet6 DAD created tunnel states based on old pf rules
	${SUDO} /sbin/pfctl -Fs
	@date >$@

.for n in ${N1} ${N2}

REGRESS_TARGETS +=	run-regress-ping-local-${n}
run-regress-ping-local-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping localhost in routing domain ${n}.
	/sbin/ping -n -w 1 -c 1 -V ${n} 127.0.0.1

REGRESS_TARGETS +=	run-regress-ping-src-${n}
run-regress-ping-src-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping source address in local routing domain.  SRC_TUNNEL4_${n}
	/sbin/ping -n -w 1 -c 1 -V ${n} ${SRC_TUNNEL4_${n}}

REGRESS_TARGETS +=	run-regress-ping-dst-${n}
run-regress-ping-dst-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping destination address in other routing domain.  DST_TUNNEL4_${n}
	/sbin/ping -n -w 1 -c 1 -V ${n} ${DST_TUNNEL4_${n}}

REGRESS_TARGETS +=	run-regress-ping-tunnel4-${n}
run-regress-ping-tunnel4-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping IPv4 address through IPv4 tunnel.  TUNNEL4_DST_ADDR4_${n}
	/sbin/ping -n -w 1 -c 1 -V ${n} ${TUNNEL4_DST_ADDR4_${n}}

REGRESS_TARGETS +=	run-regress-ping-tunnel6-${n}
run-regress-ping-tunnel6-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping IPv4 address through IPv6 tunnel.  TUNNEL6_DST_ADDR4_${n}
	/sbin/ping -n -w 1 -c 1 -V ${n} ${TUNNEL6_DST_ADDR4_${n}}

REGRESS_TARGETS +=	run-regress-ping6-local-${n}
run-regress-ping6-local-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping localhost in routing domain ${n}.
	/sbin/ping6 -n -w 1 -c 1 -V ${n} ::1

REGRESS_TARGETS +=	run-regress-ping6-src-${n}
run-regress-ping6-src-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping source address in local routing domain.  SRC_TUNNEL6_${n}
	/sbin/ping6 -n -w 1 -c 1 -V ${n} ${SRC_TUNNEL6_${n}}

REGRESS_TARGETS +=	run-regress-ping6-dst-${n}
run-regress-ping6-dst-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping destination address in other routing domain.  DST_TUNNEL6_${n}
	/sbin/ping6 -n -w 1 -c 1 -V ${n} ${DST_TUNNEL6_${n}}

REGRESS_TARGETS +=	run-regress-ping6-tunnel4-${n}
run-regress-ping6-tunnel4-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping IPv6 address through IPv4 tunnel.  TUNNEL4_DST_ADDR6_${n}
	/sbin/ping6 -n -w 1 -c 1 -V ${n} ${TUNNEL4_DST_ADDR6_${n}}

REGRESS_TARGETS +=	run-regress-ping6-tunnel6-${n}
run-regress-ping6-tunnel6-${n}: stamp-setup stamp-pfctl
	@echo '\n======== $@ ========'
	# Ping IPv6 address through IPv6 tunnel.  TUNNEL6_DST_ADDR6_${n}
	/sbin/ping6 -n -w 1 -c 1 -V ${n} ${TUNNEL6_DST_ADDR6_${n}}

.endfor

REGRESS_TARGETS +=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

CLEANFILES +=	addr.py *.pyc *.log stamp-*

.include <bsd.regress.mk>

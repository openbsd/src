#	$OpenBSD: Makefile,v 1.3 2017/10/18 19:41:31 bluhm Exp $

SYSCTL_ETHERIP !=	sysctl net.inet.etherip.allow

.if ${SYSCTL_ETHERIP:C/.*=//} != 1
regress:
	@echo "${SYSCTL_ETHERIP}"
	@echo set this sysctl to 1 for additional tests
	@echo SKIPPED
.endif

# configure interface numbers and routing domains
N1 =		11
N2 =		12
NUMS =		${N1} ${N2}
IFS =		bridge vether etherip pair

CLEANFILES =	stamp-*

.PHONY: check-interfaces check-rdomains ifconfig unconfig

check-interfaces:
	# check if interfaces are busy
.for i in ${IFS}
.for n in ${NUMS}
	@if ! /sbin/ifconfig ${i}${n} 2>&1 | grep -q "no such interface"; then\
	    echo interface ${i}${n} is already used >&2; exit 1; fi
.endfor
.endfor

check-rdomains:
	# check if rdomains are busy
.for n in ${NUMS}
	@if /sbin/ifconfig | grep -v '^lo${n}:' | grep ' rdomain ${n} '; then\
	    echo routing domain ${n} is already used >&2; exit 1; fi
.endfor

ifconfig: check-interfaces check-rdomains
	# create and configure interfaces
.for i in ${IFS}
.for n in ${NUMS}
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
.for i in ${IFS}
.for n in ${NUMS}
	-${SUDO} /sbin/ifconfig ${i}${n} destroy
.endfor
.endfor
	rm -f stamp-setup

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} ifconfig
	date >$@

REGRESS_TARGETS +=	run-regress-ping-1-2
run-regress-ping-1-2: stamp-setup
	@echo '\n======== $@ ========'
	/sbin/ping -w 1 -c 1 -V ${N1} 192.168.0.2

REGRESS_TARGETS +=	run-regress-ping-2-1
run-regress-ping-2-1: stamp-setup
	@echo '\n======== $@ ========'
	/sbin/ping -w 1 -c 1 -V ${N2} 192.168.0.1

REGRESS_TARGETS +=	run-regress-cleanup
run-regress-cleanup: stamp-setup
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>

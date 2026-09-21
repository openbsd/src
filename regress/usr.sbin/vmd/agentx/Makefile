#	$OpenBSD: Makefile,v 1.2 2026/09/21 23:17:57 jan Exp $

.if ${MACHINE} != "amd64"
regress:
	# vmd(8) exists only on amd64
	@echo SKIPPED
.else

.include <bsd.own.mk>

.if ! (make(clean) || make(cleandir) || make(obj))
VMMDEV_FAIL !=		${SUDO} /bin/sh -c 'true </dev/vmm'; echo $$?
.endif

.if defined(VMMDEV_FAIL) && "${VMMDEV_FAIL}" != "0"
regress:
	# vmm(4) is not available
	@echo SKIPPED
.else

VMD ?=			/usr/sbin/vmd
SNMPD ?=		/usr/sbin/snmpd
SNMP ?=			/usr/bin/snmp

SNMPDCONF =		${.OBJDIR}/snmpd.conf
VMDCONF =		${.OBJDIR}/vmd.conf

VMMIB =			1.3.6.1.2.1.236
SNMPGET =		${SNMP} get -Ov -v2c -c public 127.0.0.1

REGRESS_ROOT_TARGETS =	start stop cleanup
REGRESS_SETUP_ONCE =	start
REGRESS_CLEANUP =	cleanup

CLEANFILES =		snmpd.conf vmd.conf

${SNMPDCONF}:
	echo 'listen on 127.0.0.1 snmpv2c'	> $@
	echo 'agentx'				>> $@
	echo 'read-only community public'	>> $@

# The vm is disabled, so vmd(8) exports it without ever starting it.
${VMDCONF}:
	echo 'agentx'				> $@
	echo 'vm "regress-vm" {'		>> $@
	echo '	disable'			>> $@
	echo '	memory 1G'			>> $@
	echo '	boot /bsd.rd'			>> $@
	echo '}'				>> $@

start: stop ${SNMPDCONF} ${VMDCONF}
	-${SUDO} rcctl stop snmpd vmd
	${SUDO} ${SNMPD} -f ${SNMPDCONF}
	${SUDO} ${VMD} -f ${VMDCONF}
	# vmd(8) connects and registers asynchronously, wait for the subtree
	@i=0; \
	# vmNumber.0
	until ${SNMPGET} ${VMMIB}.1.2.0 2>/dev/null | grep -q 'INTEGER: 1'; do \
		i=$$((i + 1)); \
		if [ $$i -ge 100 ]; then \
			echo "vmd did not register at snmpd" >&2; \
			exit 1; \
		fi; \
		sleep 0.2; \
	done

stop:
	-${SUDO} pkill -xf '${VMD} -f ${VMDCONF}'
	-${SUDO} pkill -xf '${SNMPD} -f ${SNMPDCONF}'

# hand the daemons back to rc(8)
cleanup: stop
	-${SUDO} rcctl restart snmpd vmd || true

REGRESS_TARGETS += run-tests
run-tests:
	# vmHvSoftware.0
	test "$$(${SNMPGET} .${VMMIB}.1.1.1.0)" = "STRING: OpenBSD VMM"
	# vmHvVersion.0 is kern.osrelease
	test "$$(${SNMPGET} .${VMMIB}.1.1.2.0)" = "STRING: $$(sysctl -n kern.osrelease)"
	# vmName.1
	test "$$(${SNMPGET} .${VMMIB}.1.4.1.2.1)" = "STRING: regress-vm"
	# vmMaxMem.1
	test "$$(${SNMPGET} .${VMMIB}.1.4.1.15.1)" = "INTEGER: 1024"

.endif
.endif

.include <bsd.regress.mk>

#	$OpenBSD: Makefile,v 1.5 2017/07/07 23:15:27 bluhm Exp $

MOUNT_NOSUID != mount | grep ^$$(df -P /tmp | tail -1 | awk '{ print $$1 }') |\
    grep nosuid || true

.if ! empty (MOUNT_NOSUID)
regress:
	echo '${MOUNT_NOSUID}'
	echo temporary directory is mounted nosuid, test needs set user id
	echo SKIPPED
.endif

CFLAGS			+= -Wall -Wformat -pedantic

REGRESS_TARGETS+=	run-regress-setuid_none
REGRESS_TARGETS+=	run-regress-setgid_none
REGRESS_TARGETS+=	run-regress-setuid
REGRESS_TARGETS+=	run-regress-setgid
REGRESS_TARGETS+=	run-regress-seteuid
REGRESS_TARGETS+=	run-regress-setegid
REGRESS_TARGETS+=	run-regress-setuid_child
REGRESS_TARGETS+=	run-regress-setgid_child
REGRESS_TARGETS+=	run-regress-setresuid
REGRESS_TARGETS+=	run-regress-setresgid
REGRESS_TARGETS+=	run-regress-suidexec-on-inherit-on
REGRESS_TARGETS+=	run-regress-suidexec-on-inherit-inherit
REGRESS_TARGETS+=	run-regress-suidexec-off-on-inherit
REGRESS_TARGETS+=	run-regress-suidexec-off-off-on
REGRESS_TARGETS+=	run-regress-sgidexec-on-inherit-on
REGRESS_TARGETS+=	run-regress-sgidexec-on-inherit-inherit
REGRESS_TARGETS+=	run-regress-sgidexec-off-on-inherit
REGRESS_TARGETS+=	run-regress-sgidexec-off-off-on
REGRESS_TARGETS+=	run-regress-suidexec-real-exec-inherit
REGRESS_TARGETS+=	run-regress-suidexec-effective-exec-inherit
REGRESS_TARGETS+=	run-regress-suidexec-saved-exec-inherit
REGRESS_TARGETS+=	run-regress-sgidexec-real-exec-inherit
REGRESS_TARGETS+=	run-regress-sgidexec-effective-exec-inherit
REGRESS_TARGETS+=	run-regress-sgidexec-saved-exec-inherit

BINFILES+=	setuid_none
BINFILES+=	setgid_none
BINFILES+=	setuid
BINFILES+=	setgid
BINFILES+=	seteuid
BINFILES+=	setegid
BINFILES+=	setuid_child
BINFILES+=	setgid_child
BINFILES+=	setresuid
BINFILES+=	setresgid
BINFILES+=	setresuid_real_exec
BINFILES+=	setresuid_effective_exec
BINFILES+=	setresuid_saved_exec
BINFILES+=	suidexec_none
BINFILES+=	suidexec
BINFILES+=	sgidexec
BINFILES+=	setresgid_real_exec
BINFILES+=	setresgid_effective_exec
BINFILES+=	setresgid_saved_exec
BINFILES+=	sgidexec_none
BINFILES+=	suidexec_inherit
BINFILES+=	sgidexec_inherit
BINFILES+=	setuid_exec_inherit
BINFILES+=	setgid_exec_inherit

CLEANFILES+=	*.o ${BINFILES}

.ifmake !clean && !cleandir && !obj
DIR !!=		mktemp -d -t setuid-regress-XXXXXXXXXX
.BEGIN:
	@chmod 755 ${DIR}
.END:
	@rm -rf -- ${DIR}
.endif

run-regress-setuid_none: install-setuid_none
	cd ${DIR} && ./setuid_none
run-regress-setgid_none: install-setgid_none
	cd ${DIR} && ./setgid_none
run-regress-setuid: install-setuid
	cd ${DIR} && ${SUDO} ./setuid
run-regress-setgid: install-setgid
	cd ${DIR} && ${SUDO} ./setgid
run-regress-seteuid: install-seteuid
	cd ${DIR} && ${SUDO} ./seteuid
run-regress-setegid: install-setegid
	cd ${DIR} && ${SUDO} ./setegid
run-regress-setuid_child: install-setuid_child
	cd ${DIR} && ${SUDO} ./setuid_child
run-regress-setgid_child: install-setgid_child
	cd ${DIR} && ${SUDO} ./setgid_child
run-regress-setresuid: install-setresuid
	cd ${DIR} && ${SUDO} ./setresuid
run-regress-setresgid: install-setresgid
	cd ${DIR} && ${SUDO} ./setresgid

run-regress-suidexec-on-inherit-on: install
	cd ${DIR} && ${SUDO} ./suidexec ./suidexec_inherit ./suidexec
run-regress-suidexec-off-on-inherit: install
	cd ${DIR} && ${SUDO} ./suidexec_none ./suidexec ./suidexec_inherit
run-regress-suidexec-on-inherit-inherit: install
	cd ${DIR} && ${SUDO} ./suidexec ./suidexec_inherit ./suidexec_inherit
run-regress-suidexec-off-off-on: install
	cd ${DIR} && ${SUDO} ./suidexec_none ./suidexec_none ./suidexec
run-regress-suidexec-real-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresuid_real_exec ./setuid_exec_inherit
run-regress-suidexec-effective-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresuid_effective_exec ./setuid_exec_inherit
run-regress-suidexec-saved-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresuid_saved_exec ./setuid_exec_inherit

run-regress-sgidexec-on-inherit-on: install
	cd ${DIR} && ${SUDO} ./sgidexec ./sgidexec_inherit ./sgidexec
run-regress-sgidexec-off-on-inherit: install
	cd ${DIR} && ${SUDO} ./sgidexec_none ./sgidexec ./sgidexec_inherit
run-regress-sgidexec-on-inherit-inherit: install
	cd ${DIR} && ${SUDO} ./sgidexec ./sgidexec_inherit ./sgidexec_inherit
run-regress-sgidexec-off-off-on: install
	cd ${DIR} && ${SUDO} ./sgidexec_none ./sgidexec_none ./sgidexec
run-regress-sgidexec-real-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresgid_real_exec ./setgid_exec_inherit
run-regress-sgidexec-effective-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresgid_effective_exec ./setgid_exec_inherit
run-regress-sgidexec-saved-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresgid_saved_exec ./setgid_exec_inherit

install-suidexec: suidexec
	@${INSTALL} -o nobody -m 4555 suidexec ${DIR}
install-sgidexec: sgidexec
	@${INSTALL} -g nobody -m 2555 sgidexec ${DIR}
.for f in ${BINFILES}
install: install-${f}
install-${f}: ${f}
	@${INSTALL} $f ${DIR}
.endfor

.include <bsd.regress.mk>

#	$OpenBSD: Makefile,v 1.7 2026/11/09 14:48:51 bluhm Exp $

.if ! (make(clean) || make(cleandir) || make(obj))

MOUNT_TMP !!= mount | grep ^$$(df -P /tmp | tail -1 | awk '{ print $$1 }')

.if "${MOUNT_TMP:M*nosuid*}" != ""
regress:
	echo '${MOUNT_TMP}'
	echo temporary directory is mounted nosuid, test needs set user id
	echo SKIPPED
.endif

DIR !?=		mktemp -d -t regress-setuid-XXXXXXXXXX
MAKE +=		DIR=${DIR}
REGRESS_SETUP =		setup-dir
setup-dir:
	chmod 755 ${DIR}
REGRESS_CLEANUP =	cleanup-dir
cleanup-dir:
	rm -rf -- ${DIR}

.endif

CFLAGS =		-Wall -Wformat -pedantic

REGRESS_TARGETS=
REGRESS_TARGETS+=	run-setuid_none
REGRESS_TARGETS+=	run-setgid_none
REGRESS_TARGETS+=	run-setuid
REGRESS_TARGETS+=	run-setgid
REGRESS_TARGETS+=	run-seteuid
REGRESS_TARGETS+=	run-setegid
REGRESS_TARGETS+=	run-setuid_child
REGRESS_TARGETS+=	run-setgid_child
REGRESS_TARGETS+=	run-setresuid
REGRESS_TARGETS+=	run-setresgid
REGRESS_TARGETS+=	run-suidexec-on-inherit-on
REGRESS_TARGETS+=	run-suidexec-on-inherit-inherit
REGRESS_TARGETS+=	run-suidexec-off-on-inherit
REGRESS_TARGETS+=	run-suidexec-off-off-on
REGRESS_TARGETS+=	run-sgidexec-on-inherit-on
REGRESS_TARGETS+=	run-sgidexec-on-inherit-inherit
REGRESS_TARGETS+=	run-sgidexec-off-on-inherit
REGRESS_TARGETS+=	run-sgidexec-off-off-on
REGRESS_TARGETS+=	run-suidexec-real-exec-inherit
REGRESS_TARGETS+=	run-suidexec-effective-exec-inherit
REGRESS_TARGETS+=	run-suidexec-saved-exec-inherit
REGRESS_TARGETS+=	run-sgidexec-real-exec-inherit
REGRESS_TARGETS+=	run-sgidexec-effective-exec-inherit
REGRESS_TARGETS+=	run-sgidexec-saved-exec-inherit

PROGS=
PROGS+=	setuid_none
PROGS+=	setgid_none
PROGS+=	setuid
PROGS+=	setgid
PROGS+=	seteuid
PROGS+=	setegid
PROGS+=	setuid_child
PROGS+=	setgid_child
PROGS+=	setresuid
PROGS+=	setresgid
PROGS+=	setresuid_real_exec
PROGS+=	setresuid_effective_exec
PROGS+=	setresuid_saved_exec
PROGS+=	suidexec_none
PROGS+=	suidexec
PROGS+=	sgidexec
PROGS+=	setresgid_real_exec
PROGS+=	setresgid_effective_exec
PROGS+=	setresgid_saved_exec
PROGS+=	sgidexec_none
PROGS+=	suidexec_inherit
PROGS+=	sgidexec_inherit
PROGS+=	setuid_exec_inherit
PROGS+=	setgid_exec_inherit

run-setuid_none: install-setuid_none
	cd ${DIR} && ./setuid_none
run-setgid_none: install-setgid_none
	cd ${DIR} && ./setgid_none
run-setuid: install-setuid
	cd ${DIR} && ${SUDO} ./setuid
run-setgid: install-setgid
	cd ${DIR} && ${SUDO} ./setgid
run-seteuid: install-seteuid
	cd ${DIR} && ${SUDO} ./seteuid
run-setegid: install-setegid
	cd ${DIR} && ${SUDO} ./setegid
run-setuid_child: install-setuid_child
	cd ${DIR} && ${SUDO} ./setuid_child
run-setgid_child: install-setgid_child
	cd ${DIR} && ${SUDO} ./setgid_child
run-setresuid: install-setresuid
	cd ${DIR} && ${SUDO} ./setresuid
run-setresgid: install-setresgid
	cd ${DIR} && ${SUDO} ./setresgid

run-suidexec-on-inherit-on: install
	cd ${DIR} && ${SUDO} ./suidexec ./suidexec_inherit ./suidexec
run-suidexec-off-on-inherit: install
	cd ${DIR} && ${SUDO} ./suidexec_none ./suidexec ./suidexec_inherit
run-suidexec-on-inherit-inherit: install
	cd ${DIR} && ${SUDO} ./suidexec ./suidexec_inherit ./suidexec_inherit
run-suidexec-off-off-on: install
	cd ${DIR} && ${SUDO} ./suidexec_none ./suidexec_none ./suidexec
run-suidexec-real-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresuid_real_exec ./setuid_exec_inherit
run-suidexec-effective-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresuid_effective_exec ./setuid_exec_inherit
run-suidexec-saved-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresuid_saved_exec ./setuid_exec_inherit

run-sgidexec-on-inherit-on: install
	cd ${DIR} && ${SUDO} ./sgidexec ./sgidexec_inherit ./sgidexec
run-sgidexec-off-on-inherit: install
	cd ${DIR} && ${SUDO} ./sgidexec_none ./sgidexec ./sgidexec_inherit
run-sgidexec-on-inherit-inherit: install
	cd ${DIR} && ${SUDO} ./sgidexec ./sgidexec_inherit ./sgidexec_inherit
run-sgidexec-off-off-on: install
	cd ${DIR} && ${SUDO} ./sgidexec_none ./sgidexec_none ./sgidexec
run-sgidexec-real-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresgid_real_exec ./setgid_exec_inherit
run-sgidexec-effective-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresgid_effective_exec ./setgid_exec_inherit
run-sgidexec-saved-exec-inherit: install
	cd ${DIR} && ${SUDO} ./setresgid_saved_exec ./setgid_exec_inherit

install-suidexec: ${DIR}/suidexec
${DIR}/suidexec: suidexec
	${SUDO} ${INSTALL} -o nobody -m 4555 suidexec ${DIR}
install-sgidexec: ${DIR}/sgidexec
${DIR}/sgidexec: sgidexec
	${SUDO} ${INSTALL} -g nobody -m 2555 sgidexec ${DIR}
.for f in ${PROGS}
install: install-${f}
install-${f}: ${DIR}/${f}
${DIR}/${f}: ${f}
	${SUDO} ${INSTALL} $f ${DIR}
.endfor

.include <bsd.regress.mk>

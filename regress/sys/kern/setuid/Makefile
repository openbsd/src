#	$OpenBSD: Makefile,v 1.1 2014/08/27 07:36:14 blambert Exp $

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

CLEANFILES+=	*.o
CLEANFILES+=	setuid_none
CLEANFILES+=	setgid_none
CLEANFILES+=	setuid
CLEANFILES+=	setgid
CLEANFILES+=	seteuid
CLEANFILES+=	setegid
CLEANFILES+=	setuid_child
CLEANFILES+=	setgid_child
CLEANFILES+=	setresuid
CLEANFILES+=	setresgid
CLEANFILES+=	setresuid_real_exec
CLEANFILES+=	setresuid_effective_exec
CLEANFILES+=	setresuid_saved_exec
CLEANFILES+=	suidexec_none
CLEANFILES+=	suidexec
CLEANFILES+=	sgidexec
CLEANFILES+=	setresgid_real_exec
CLEANFILES+=	setresgid_effective_exec
CLEANFILES+=	setresgid_saved_exec
CLEANFILES+=	sgidexec_none
CLEANFILES+=	suidexec_inherit
CLEANFILES+=	sgidexec_inherit
CLEANFILES+=	setuid_exec_inherit
CLEANFILES+=	setgid_exec_inherit

run-regress-setuid_none: setuid_none
	./setuid_none
run-regress-setgid_none: setgid_none
	./setgid_none
run-regress-setuid: setuid
	${SUDO} ./setuid
run-regress-setgid: setgid
	${SUDO} ./setgid
run-regress-seteuid: seteuid
	${SUDO} ./seteuid
run-regress-setegid: setegid
	${SUDO} ./setegid
run-regress-setuid_child: setuid_child
	${SUDO} ./setuid_child
run-regress-setgid_child: setgid_child
	${SUDO} ./setgid_child
run-regress-setresuid: setresuid
	${SUDO} ./setresuid
run-regress-setresgid: setresgid
	${SUDO} ./setresgid

run-regress-suidexec-on-inherit-on: suidexec-install
	${SUDO} ./suidexec ./suidexec_inherit ./suidexec
run-regress-suidexec-off-on-inherit: suidexec-install
	${SUDO} ./suidexec_none ./suidexec ./suidexec_inherit
run-regress-suidexec-on-inherit-inherit: suidexec-install
	${SUDO} ./suidexec ./suidexec_inherit ./suidexec_inherit
run-regress-suidexec-off-off-on: suidexec-install
	${SUDO} ./suidexec_none ./suidexec_none ./suidexec
run-regress-suidexec-real-exec-inherit: suidexec-install
	${SUDO} ./setresuid_real_exec ./setuid_exec_inherit
run-regress-suidexec-effective-exec-inherit: suidexec-install
	${SUDO} ./setresuid_effective_exec ./setuid_exec_inherit
run-regress-suidexec-saved-exec-inherit: suidexec-install
	${SUDO} ./setresuid_saved_exec ./setuid_exec_inherit

run-regress-sgidexec-on-inherit-on: sgidexec-install
	${SUDO} ./sgidexec ./sgidexec_inherit ./sgidexec
run-regress-sgidexec-off-on-inherit: sgidexec-install
	${SUDO} ./sgidexec_none ./sgidexec ./sgidexec_inherit
run-regress-sgidexec-on-inherit-inherit: sgidexec-install
	${SUDO} ./sgidexec ./sgidexec_inherit ./sgidexec_inherit
run-regress-sgidexec-off-off-on: sgidexec-install
	${SUDO} ./sgidexec_none ./sgidexec_none ./sgidexec
run-regress-sgidexec-real-exec-inherit: sgidexec-install
	${SUDO} ./setresgid_real_exec ./setgid_exec_inherit
run-regress-sgidexec-effective-exec-inherit: sgidexec-install
	${SUDO} ./setresgid_effective_exec ./setgid_exec_inherit
run-regress-sgidexec-saved-exec-inherit: sgidexec-install
	${SUDO} ./setresgid_saved_exec ./setgid_exec_inherit

suidexec-install: suidexec suidexec_none suidexec_inherit setresuid_real_exec setresuid_effective_exec setresuid_saved_exec setuid_exec_inherit
	${SUDO} chown nobody:nobody ./suidexec
	${SUDO} chmod 4555 ./suidexec

sgidexec-install: sgidexec sgidexec_none sgidexec_inherit setresgid_real_exec setresgid_effective_exec setresgid_saved_exec setgid_exec_inherit
	${SUDO} chown nobody:nobody ./sgidexec
	${SUDO} chmod 2555 ./sgidexec

.include <bsd.regress.mk>

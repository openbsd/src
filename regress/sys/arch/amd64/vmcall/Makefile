# $OpenBSD: Makefile,v 1.1 2026/02/16 13:05:14 hshoexer Exp $

.if "${MACHINE_ARCH}" != amd64
regress:
	# MACHINE_ARCH is not amd64
	@echo SKIPPED
.else

PROGS =			vmcall
WARNINGS =		yes

REGRESS_TARGETS =	run-regress-vmcall
run-regress-vmcall:	vmcall
	./vmcall

.if ! (make(clean) || make(cleandir) || make(obj))
.include <bsd.own.mk>
VMMODE !=	/sbin/sysctl -n machdep.vmmode
HYPVERVISOR!=	/sbin/sysctl -n hw.vendor

# Only run test on vmm(4) guests
.if "${VMMODE}" == "host" || "${HYPVERVISOR}" != "OpenBSD"
REGRESS_SKIP_TARGETS =		${REGRESS_TARGETS}
.endif
.endif
.endif

.include <bsd.regress.mk>

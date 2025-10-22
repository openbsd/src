# $OpenBSD: Makefile,v 1.1 2025/10/22 14:22:00 hshoexer Exp $

.if "${MACHINE_ARCH}" != amd64
regress:
	# MACHINE_ARCH is not amd64
	@echo SKIPPED
.else

PROGS =			seves_mmio
WARNINGS =		yes

REGRESS_TARGETS =	run-regress-seves_mmio
run-regress-seves_mmio: seves_mmio
	${SUDO} ./seves_mmio

.if ! (make(clean) || make(cleandir) || make(obj))
.include <bsd.own.mk>
ALLOWAPERTURE !=	/sbin/sysctl -n machdep.allowaperture
SEVES !=		/sbin/sysctl -n machdep.vmmode

.if "${ALLOWAPERTURE}" == 0 || "${SEVES}" != "SEV-ES"
REGRESS_SKIP_TARGETS =		${REGRESS_TARGETS}
.endif
.endif
.endif

.include <bsd.regress.mk>

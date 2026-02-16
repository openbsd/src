#	$OpenBSD: Makefile,v 1.6 2026/02/16 13:08:57 hshoexer Exp $

.if ${MACHINE_ARCH} == "amd64"
SUBDIR +=	ddb
SUBDIR +=	dump_tables
SUBDIR +=	fpu
SUBDIR +=	seves_mmio
SUBDIR +=	vmcall
SUBDIR +=	vmmcall
SUBDIR +=	vmm

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture amd64 cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>

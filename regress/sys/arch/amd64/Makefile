#	$OpenBSD: Makefile,v 1.4 2025/10/22 14:22:00 hshoexer Exp $

.if ${MACHINE_ARCH} == "amd64"
SUBDIR +=	ddb
SUBDIR +=	dump_tables
SUBDIR +=	fpu
SUBDIR +=	seves_mmio
SUBDIR +=	vmm

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture amd64 cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>

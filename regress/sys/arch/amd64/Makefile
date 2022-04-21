#	$OpenBSD: Makefile,v 1.2 2022/04/21 19:21:04 bluhm Exp $

.if ${MACHINE_ARCH} == "amd64"
SUBDIR =
SUBDIR +=	dump_tables
SUBDIR +=	fpu
SUBDIR +=	vmm

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture amd64 cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.1 2018/08/21 18:42:40 bluhm Exp $

.if ${MACHINE_ARCH} == "amd64"
SUBDIR =
SUBDIR +=	fpu

.elif make(regress) || make(all)
${.TARGETS}:
	@echo Regress for architecture amd64 cannot run on ${MACHINE_ARCH}.
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>

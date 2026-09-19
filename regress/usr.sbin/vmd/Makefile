#	$OpenBSD: Makefile,v 1.4 2026/09/19 19:52:30 jan Exp $

.if ${MACHINE} == "amd64"
SUBDIR =
SUBDIR += agentx config diskfmt

.elif make(regress) || make(all)
${.TARGETS}:
	# vmd(8) exists only on amd64, but not on ${MACHINE}
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.2 2019/01/29 21:08:12 bluhm Exp $

.if ${MACHINE} == "amd64"
SUBDIR =
SUBDIR += config

.elif make(regress) || make(all)
${.TARGETS}:
	# vmd(8) exists only on amd64, but not on ${MACHINE}
	@echo SKIPPED
.endif

install:

.include <bsd.subdir.mk>

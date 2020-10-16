# $OpenBSD: Makefile.inc,v 1.1 2020/10/16 21:34:59 bluhm Exp $

.if ${MACHINE} != "amd64"
regress:
	# Need syscall asm for this arch
	@echo SKIPPED
.endif

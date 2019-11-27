#	$OpenBSD: Makefile,v 1.1 2019/11/27 17:15:36 mortimer Exp $

.if ${MACHINE} != "amd64"
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
        # Need syscall asm for this arch
        @echo SKIPPED
.include <bsd.regress.mk>
.else
SUBDIR += main
SUBDIR += libgadgetsyscall
SUBDIR += lib
.include <bsd.subdir.mk>
.endif

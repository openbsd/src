#	$OpenBSD: Makefile,v 1.5 2003/07/31 22:46:59 david Exp $

NOMAN=
PROG=atexit_test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libc
CLEANFILES+= invalid.out valid.out

run-regress-atexit_test: ${PROG}
	./${PROG} -valid 2>${.OBJDIR}/valid.out
	cmp -s ${.OBJDIR}/valid.out ${.CURDIR}/valid.ok
	./${PROG} -invalid-atexit 2>${.OBJDIR}/invalid.out
	cmp -s ${.OBJDIR}/invalid.out ${.CURDIR}/invalid.ok
	./${PROG} -invalid-cleanup 2>${.OBJDIR}/invalid.out
	cmp -s ${.OBJDIR}/invalid.out ${.CURDIR}/invalid.ok

.include <bsd.regress.mk>

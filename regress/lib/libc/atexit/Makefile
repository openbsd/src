#	$OpenBSD: Makefile,v 1.4 2002/10/21 20:40:50 mickey Exp $

NOMAN=
PROG=atexit_test
CPPFLAGS+=-I${.CURDIR}/../../../../lib/libc

run-regress-atexit_test: ${PROG}
	./${PROG} -valid 2>${.OBJDIR}/valid.out
	cmp -s ${.OBJDIR}/valid.out ${.CURDIR}/valid.ok
	./${PROG} -invalid-atexit 2>${.OBJDIR}/invalid.out
	cmp -s ${.OBJDIR}/invalid.out ${.CURDIR}/invalid.ok
	./${PROG} -invalid-cleanup 2>${.OBJDIR}/invalid.out
	cmp -s ${.OBJDIR}/invalid.out ${.CURDIR}/invalid.ok

.include <bsd.regress.mk>

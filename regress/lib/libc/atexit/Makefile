#	$OpenBSD: Makefile,v 1.2 2002/09/02 19:59:51 avsm Exp $

NOMAN=
PROG=atexit_test

run-regress-atexit_test: ${PROG}
	./${PROG} -valid 2>${.OBJDIR}/valid.out
	cmp -s ${.OBJDIR}/valid.out ${.CURDIR}/valid.ok
	./${PROG} -invalid 2>${.OBJDIR}/invalid.out
	cmp -s ${.OBJDIR}/invalid.out ${.CURDIR}/invalid.ok

.include <bsd.regress.mk>

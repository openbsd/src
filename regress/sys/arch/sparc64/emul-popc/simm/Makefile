#	$OpenBSD: Makefile,v 1.3 2003/07/12 04:07:05 jason Exp $

# -O2 breaks this
CFLAGS=-g
PROG=	simm13
SRCS=simm13.c simm13_asm.S
REGRESS_TARGETS=simm13_check

simm13.c: genc.pl
	perl ${.CURDIR}/genc.pl > $@

simm13_asm.S: genasm.pl
	perl ${.CURDIR}/genasm.pl > $@

simm13_check: simm13
	${.OBJDIR}/simm13 | diff -u - ${.CURDIR}/simm.out

.include <bsd.regress.mk>

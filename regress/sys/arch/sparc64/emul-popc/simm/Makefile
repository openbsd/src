
# -O2 breaks this
CFLAGS=-g
PROG=	simm13
SRCS=simm13.c simm13_asm.S
REGRESS_TARGETS=simm13_check

simm13.c: genc.pl
	perl $> > $@

simm13_asm.S: genasm.pl
	perl $> > $@

simm13_check: simm13
	${.OBJDIR}/simm13 | diff -u - ${.CURDIR}/simm.out

.include <bsd.regress.mk>

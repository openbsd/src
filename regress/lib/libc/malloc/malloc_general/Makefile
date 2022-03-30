#	$OpenBSD: Makefile,v 1.6 2022/03/30 05:22:31 anton Exp $

REGRESS_TARGETS=        t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11
PROG=			malloc_general

.include <bsd.regress.mk>

t1:	malloc_general
	MALLOC_OPTIONS=cfgju ${.OBJDIR}/malloc_general

t2:	malloc_general
	MALLOC_OPTIONS=cfgjuC ${.OBJDIR}/malloc_general

t3:	malloc_general
	MALLOC_OPTIONS=cfgjuJ ${.OBJDIR}/malloc_general

t4:	malloc_general
	MALLOC_OPTIONS=cfgjuF ${.OBJDIR}/malloc_general

t5:	malloc_general
	MALLOC_OPTIONS=cfgjuG ${.OBJDIR}/malloc_general

t6:	malloc_general
	MALLOC_OPTIONS=cfgjuS ${.OBJDIR}/malloc_general

t7:	malloc_general
	MALLOC_OPTIONS=cfgjuFGJ ${.OBJDIR}/malloc_general

t8:	malloc_general
	MALLOC_OPTIONS=cfgjuCJ ${.OBJDIR}/malloc_general

t9:	malloc_general
	MALLOC_OPTIONS=cfgjuCJJ ${.OBJDIR}/malloc_general

t10:	malloc_general
	MALLOC_OPTIONS=cfgjuJS ${.OBJDIR}/malloc_general

t11:	malloc_general
	MALLOC_OPTIONS=cfgjuFGJJ ${.OBJDIR}/malloc_general

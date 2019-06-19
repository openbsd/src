#	$OpenBSD: Makefile,v 1.4 2019/06/04 05:30:39 otto Exp $

REGRESS_TARGETS=        t1 t2 t3 t4 t5 t6 t7
PROG=			malloc_general

.include <bsd.regress.mk>

t1:	malloc_general
	MALLOC_OPTIONS=cfgju ${.OBJDIR}/malloc_general

t2:	malloc_general
	MALLOC_OPTIONS=cfgjuC ${.OBJDIR}//malloc_general

t3:	malloc_general
	MALLOC_OPTIONS=cfgjuJ ${.OBJDIR}//malloc_general

t4:	malloc_general
	MALLOC_OPTIONS=cfgjuF ${.OBJDIR}//malloc_general

t5:	malloc_general
	MALLOC_OPTIONS=cfgjuG ${.OBJDIR}//malloc_general

t6:	malloc_general
	MALLOC_OPTIONS=cfgjuS ${.OBJDIR}//malloc_general

t7:	malloc_general
	MALLOC_OPTIONS=cfgjuFGJ ${.OBJDIR}//malloc_general

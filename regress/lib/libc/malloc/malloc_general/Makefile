#	$OpenBSD: Makefile,v 1.2 2017/01/24 15:32:20 otto Exp $

REGRESS_TARGETS=        t1 t2 t3 t4 t5 t6 t7

.include <bsd.regress.mk>

t1:	malloc_general
	${.OBJDIR}/malloc_general ""

t2:	malloc_general
	${.OBJDIR}//malloc_general C

t3:	malloc_general
	${.OBJDIR}//malloc_general J

t4:	malloc_general
	${.OBJDIR}//malloc_general F

t5:	malloc_general
	${.OBJDIR}//malloc_general G

t6:	malloc_general
	${.OBJDIR}//malloc_general S

t7:	malloc_general
	${.OBJDIR}//malloc_general FGJ

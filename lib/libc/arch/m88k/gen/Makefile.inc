#	$OpenBSD: Makefile.inc,v 1.12 2011/07/08 22:28:33 martynas Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/04/10 21:09:06 jtc Exp $

#SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.c nan.c
#SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
#	fpsetround.c fpsetsticky.c
#SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.c nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	setjmp.S sigsetjmp.S

SRCS+=	alloca.c

#	$OpenBSD: Makefile.inc,v 1.5 2005/11/29 21:38:09 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/04/10 21:09:06 jtc Exp $

#SRCS+=	_setjmp.S fabs.S infinity.c isinf.c isnan.c ldexp.c modf.S
#SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
#	fpsetround.c fpsetsticky.c
#SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	modf.c

SRCS+=	alloca.c

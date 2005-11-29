#	$OpenBSD: Makefile.inc,v 1.5 2005/11/29 21:38:10 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c isinf.c isnan.c ldexp.c modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S

SRCS+=	alloca.c

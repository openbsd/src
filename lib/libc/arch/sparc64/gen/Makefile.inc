#	$OpenBSD: Makefile.inc,v 1.3 2005/02/01 15:12:30 millert Exp $

SRCS+=	_setjmp.S fabs.S infinity.c isinf.c isnan.c ldexp.c modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S
SRCS+=	alloca.c

#	$OpenBSD: Makefile.inc,v 1.2 2003/05/02 19:20:26 millert Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S
SRCS+=	alloca.c

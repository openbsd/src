#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:17:24 tholo Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.c isnan.c ldexp.c modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S

#	$OpenBSD: Makefile.inc,v 1.8 2008/12/09 19:52:34 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.S nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S

SRCS+=	alloca.c

#	$OpenBSD: Makefile.inc,v 1.11 2011/07/08 19:21:42 martynas Exp $

SRCS+=	_setjmp.S infinity.c ldexp.c nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fixunsdfsi.S mul.S umul.S saveregs.S setjmp.S sigsetjmp.S

SRCS+=	alloca.c

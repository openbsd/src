#	$OpenBSD: Makefile.inc,v 1.3 1996/08/19 08:16:34 tholo Exp $

SRCS+=	__main.c _setjmp.S alloca.S fabs.S frexp.S infinity.c isinf.c
SRCS+=	ldexp.S modf.S setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c

#	$OpenBSD: Makefile.inc,v 1.4 2003/05/02 19:20:26 millert Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.S ldexp.S modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	alloca.c

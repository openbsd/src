#	$OpenBSD: Makefile.inc,v 1.1 2004/08/11 17:30:59 pefo Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.S ldexp.S modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	alloca.c

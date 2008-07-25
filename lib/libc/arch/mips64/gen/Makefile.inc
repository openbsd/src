#	$OpenBSD: Makefile.inc,v 1.5 2008/07/25 08:00:01 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	setjmp.S sigsetjmp.S

SRCS+=	alloca.c

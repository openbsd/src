#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:10:18 tholo Exp $

SRCS+=	_setjmp.S fabs.S frexp.c infinity.c isinf.c ldexp.c modf.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	sigsetjmp.S

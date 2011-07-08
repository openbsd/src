#	$OpenBSD: Makefile.inc,v 1.10 2011/07/08 22:28:33 martynas Exp $

SRCS+= flt_rounds.c infinity.c ldexp.c modf.c nan.c setjmp.S _setjmp.S
SRCS+= sigsetjmp.S
SRCS+=	fabs.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c

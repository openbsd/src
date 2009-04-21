#	$OpenBSD: Makefile.inc,v 1.8 2009/04/21 09:34:50 martynas Exp $

SRCS+= flt_rounds.c infinity.c ldexp.c modf.c nan.c setjmp.S _setjmp.S
SRCS+= sigsetjmp.S
SRCS+=	fabs.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c

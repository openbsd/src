#	$OpenBSD: Makefile.inc,v 1.3 2006/10/11 03:20:56 drahn Exp $

SRCS+=	fabs.c flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c isinf.c isnan.c infinity.c \
	setjmp.S _setjmp.S sigsetjmp.S modf.c ldexp.c

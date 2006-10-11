#	$OpenBSD: Makefile.inc,v 1.2 2006/10/11 02:56:37 drahn Exp $

SRCS+=	fabs.c flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c \
	fpsetmask.c fpsetround.c fpsetsticky.c isinf.c isnan.c infinity.c \
	setjmp.S _setjmp.S sigsetjmp.S modf.c

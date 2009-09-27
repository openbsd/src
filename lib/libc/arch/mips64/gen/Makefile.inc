#	$OpenBSD: Makefile.inc,v 1.8 2009/09/27 18:20:13 miod Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	cacheflush.c

SRCS+=	alloca.c

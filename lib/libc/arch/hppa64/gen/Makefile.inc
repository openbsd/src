#	$OpenBSD: Makefile.inc,v 1.9 2011/07/08 22:28:33 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c
SRCS+=	infinity.c ldexp.c modf.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c

SRCS+=	alloca.c

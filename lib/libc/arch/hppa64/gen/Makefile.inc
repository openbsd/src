#	$OpenBSD: Makefile.inc,v 1.5 2008/12/09 19:52:33 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	infinity.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	modf.c

SRCS+=	alloca.c

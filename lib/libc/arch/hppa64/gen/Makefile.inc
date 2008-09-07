#	$OpenBSD: Makefile.inc,v 1.4 2008/09/07 20:36:07 martynas Exp $

SRCS+=	setjmp.S
SRCS+=	fabs.c frexp.c ldexp.c
SRCS+=	infinity.c nan.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c
SRCS+=	modf.c

SRCS+=	alloca.c

#	$OpenBSD: Makefile.inc,v 1.11 2011/07/08 22:28:33 martynas Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	cacheflush.c fpc_csr.c

SRCS+=	alloca.c

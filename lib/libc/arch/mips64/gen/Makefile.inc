#	$OpenBSD: Makefile.inc,v 1.12 2012/04/12 16:14:09 deraadt Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S nan.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	cacheflush.c fpc_csr.c

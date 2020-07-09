#	$OpenBSD: Makefile.inc,v 1.13 2020/07/06 13:33:05 pirofti Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.S modf.S nan.c usertc.c
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c
SRCS+=	setjmp.S sigsetjmp.S
SRCS+=	cacheflush.c fpc_csr.c

#	$OpenBSD: Makefile.inc,v 1.13 2012/04/19 19:14:56 deraadt Exp $

SRCS+=	_setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpclassifyl.c \
	fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c infinity.c isfinitel.c \
	isinfl.c isnanl.c isnormall.c ldexp.c modf.S \
	mul.S nan.c saveregs.S setjmp.S signbitl.c sigsetjmp.S umul.S

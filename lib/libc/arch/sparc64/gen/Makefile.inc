#	$OpenBSD: Makefile.inc,v 1.14 2017/10/28 21:37:28 guenther Exp $

SRCS+=	_setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpclassifyl.c \
	fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c infinity.c isfinitel.c \
	isinfl.c isnanl.c isnormall.c ldexp.c modf.S \
	mul.S nan.c setjmp.S signbitl.c sigsetjmp.S umul.S

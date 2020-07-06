#	$OpenBSD: Makefile.inc,v 1.15 2020/07/06 13:33:05 pirofti Exp $

SRCS+=	_setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpclassifyl.c \
	fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c infinity.c isfinitel.c \
	isinfl.c isnanl.c isnormall.c ldexp.c usertc.c modf.S \
	mul.S nan.c setjmp.S signbitl.c sigsetjmp.S umul.S

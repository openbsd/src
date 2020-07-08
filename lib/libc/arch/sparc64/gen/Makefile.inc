#	$OpenBSD: Makefile.inc,v 1.16 2020/07/08 09:20:28 kettenis Exp $

SRCS+=	_setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpclassifyl.c \
	fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c infinity.c isfinitel.c \
	isinfl.c isnanl.c isnormall.c ldexp.c usertc.c modf.S \
	mul.S nan.c setjmp.S signbitl.c sigsetjmp.S umul.S

CFLAGS += -Wa,-Av9b

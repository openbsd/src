#	$OpenBSD: Makefile.inc,v 1.13 2020/07/06 13:33:05 pirofti Exp $

SRCS+=	_setjmp.S fabs.S infinity.c ldexp.c modf.S nan.c setjmp.S \
	sigsetjmp.S
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c isnormall.c signbitl.c \
	usertc.c
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S

# $OpenBSD: Makefile.inc,v 1.1 2021/04/29 18:37:19 drahn Exp $
SRCS+=	ldexp.c
SRCS+=	infinity.c modf.c nan.c
SRCS+=	flt_rounds.c
SRCS+=	fpclassifyl.c
SRCS+=	fpgetmask.c fpgetround.c fpgetsticky.c
SRCS+=	fpsetmask.c fpsetround.c fpsetsticky.c
SRCS+=	usertc.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S
SRCS+=	isnanl.c signbitl.c isfinitel.c isinfl.c isnormall.c

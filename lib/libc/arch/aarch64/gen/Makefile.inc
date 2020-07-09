# $OpenBSD: Makefile.inc,v 1.2 2020/07/06 13:33:05 pirofti Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S fabs.c flt_rounds.c infinity.c
SRCS+=	ldexp.c modf.c nan.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S
SRCS+=	clz_tab.c
SRCS+=	fpgetmask.c fpgetround.c fpgetsticky.c
SRCS+=	fpsetmask.c fpsetround.c fpsetsticky.c
SRCS+=	fpclassifyl.c
SRCS+=	isfinitel.c isinfl.c isnanl.c isnormall.c
SRCS+=	signbitl.c usertc.c

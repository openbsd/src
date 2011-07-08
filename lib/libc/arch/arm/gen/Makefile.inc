# $OpenBSD: Makefile.inc,v 1.13 2011/07/08 22:28:33 martynas Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S divsi3.S fabs.c flt_rounds.c infinity.c
SRCS+=	ldexp.c modf.c nan.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S

SRCS+=	alloca.S
LSRCS+= alloca.c

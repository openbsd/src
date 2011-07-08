# $OpenBSD: Makefile.inc,v 1.12 2011/07/08 19:21:41 martynas Exp $
# $NetBSD: Makefile.inc,v 1.6 2003/08/01 17:03:47 lukem Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S divsi3.S flt_rounds.c infinity.c
SRCS+=	ldexp.c nan.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S

SRCS+=	alloca.S
LSRCS+= alloca.c

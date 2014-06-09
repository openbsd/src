# $OpenBSD: Makefile.inc,v 1.5 2014/06/09 20:47:10 miod Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

#SRCS+=	__sigaction14_sigtramp.c __sigtramp1.S

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <arch/arm/softfloat/Makefile.inc>

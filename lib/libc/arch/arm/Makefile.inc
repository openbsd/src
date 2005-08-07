# $OpenBSD: Makefile.inc,v 1.4 2005/08/07 22:12:11 espie Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

KMINCLUDES=
KMSRCS=arch/arm/gen/divsi3.S arch/arm/string/memmove.S \
	arch/arm/string/memcpy.S

#SRCS+=	__sigaction14_sigtramp.c __sigtramp1.S

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <arch/arm/softfloat/Makefile.inc>

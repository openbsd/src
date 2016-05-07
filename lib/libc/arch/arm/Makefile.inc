# $OpenBSD: Makefile.inc,v 1.6 2016/05/07 19:05:21 guenther Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

CERROR=	cerror.S

CPPFLAGS += -DSOFTFLOAT

SOFTFLOAT_BITS=32
.include <arch/arm/softfloat/Makefile.inc>

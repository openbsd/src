# $OpenBSD: Makefile.inc,v 1.2 2019/04/01 03:53:21 kettenis Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

CERROR=	cerror.S

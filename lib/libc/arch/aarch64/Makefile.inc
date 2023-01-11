# $OpenBSD: Makefile.inc,v 1.4 2023/01/11 14:38:09 deraadt Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

CERROR=	cerror.S

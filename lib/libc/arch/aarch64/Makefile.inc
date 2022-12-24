# $OpenBSD: Makefile.inc,v 1.3 2022/12/24 10:50:40 kettenis Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/07/10 04:29:06 thorpej Exp $

.include <bsd.own.mk>

LDADD+= -Wl,--execute-only

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4

CERROR=	cerror.S

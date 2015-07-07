# $OpenBSD: Makefile,v 1.4 2015/07/07 17:39:19 semarie Exp $

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= check_isw
.endif

SUBDIR += setlocale

install:

.include <bsd.subdir.mk>

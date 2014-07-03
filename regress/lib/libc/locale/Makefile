# $OpenBSD: Makefile,v 1.3 2014/07/03 21:12:24 miod Exp $

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= check_isw
.endif

install:

.include <bsd.subdir.mk>

# $OpenBSD: Makefile,v 1.5 2016/09/05 09:53:53 schwarze Exp $

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= check_isw
.endif

SUBDIR += mbrtowc setlocale wcrtomb

install:

.include <bsd.subdir.mk>

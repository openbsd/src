#	$OpenBSD: Makefile,v 1.14 2014/07/03 21:12:24 miod Exp $

SUBDIR+= exp fenv floor fpaccuracy lgamma nextafter rint round \
	 tgamma toint trivial1

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cephes
.endif

install:

.include <bsd.subdir.mk>

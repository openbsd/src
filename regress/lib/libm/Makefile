#	$OpenBSD: Makefile,v 1.15 2019/02/21 16:14:45 bluhm Exp $

SUBDIR+= exp fenv floor fpaccuracy lgamma nextafter msun rint round \
	 tgamma toint trivial1

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cephes
.endif

install:

.include <bsd.subdir.mk>

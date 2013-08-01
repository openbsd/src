#	$OpenBSD: Makefile,v 1.13 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= exp fenv floor fpaccuracy lgamma nextafter rint round \
	 tgamma toint trivial1

.if defined(REGRESS_FULL)
SUBDIR+= cephes
.endif

install:

.include <bsd.subdir.mk>

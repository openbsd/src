#	$OpenBSD: Makefile,v 1.7 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= route

.if defined(REGRESS_FULL)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

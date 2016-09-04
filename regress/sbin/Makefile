#	$OpenBSD: Makefile,v 1.9 2016/09/04 18:13:17 bluhm Exp $

SUBDIR+= isakmpd route

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.10 2017/05/30 12:03:11 bluhm Exp $

SUBDIR+= iked isakmpd route

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.12 2019/10/16 20:17:39 bluhm Exp $

SUBDIR+= ifconfig iked isakmpd route slaacd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

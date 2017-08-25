#	$OpenBSD: Makefile,v 1.11 2017/08/25 20:16:56 florian Exp $

SUBDIR+= iked isakmpd route slaacd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

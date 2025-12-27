#	$OpenBSD: Makefile,v 1.14 2025/12/27 22:14:40 kn Exp $

SUBDIR+= disklabel ifconfig iked isakmpd newfs route slaacd sysctl

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

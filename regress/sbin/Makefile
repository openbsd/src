#	$OpenBSD: Makefile,v 1.8 2014/07/03 21:12:24 miod Exp $

SUBDIR+= route

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ipsecctl pfctl
.endif

install:

.include <bsd.subdir.mk>

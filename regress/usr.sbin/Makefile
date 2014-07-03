#	$OpenBSD: Makefile,v 1.7 2014/07/03 21:12:24 miod Exp $

SUBDIR += mtree
SUBDIR += openssl
SUBDIR += relayd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

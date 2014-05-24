#	$OpenBSD: Makefile,v 1.6 2014/05/24 13:32:46 jsing Exp $

SUBDIR += mtree
SUBDIR += openssl
SUBDIR += relayd

.if defined(REGRESS_FULL)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

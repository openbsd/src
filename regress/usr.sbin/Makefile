#	$OpenBSD: Makefile,v 1.8 2014/08/20 20:54:27 bluhm Exp $

SUBDIR += mtree
SUBDIR += openssl
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

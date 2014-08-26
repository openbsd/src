#	$OpenBSD: Makefile,v 1.9 2014/08/26 17:50:07 jsing Exp $

SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

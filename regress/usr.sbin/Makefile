#	$OpenBSD: Makefile,v 1.10 2015/06/23 18:04:29 semarie Exp $

SUBDIR += httpd
SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

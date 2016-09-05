#	$OpenBSD: Makefile,v 1.12 2016/09/05 06:21:02 landry Exp $

SUBDIR += arp
SUBDIR += httpd
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

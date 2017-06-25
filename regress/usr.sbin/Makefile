#	$OpenBSD: Makefile,v 1.14 2017/06/25 22:07:55 bluhm Exp $

SUBDIR += acme-client
SUBDIR += arp
SUBDIR += httpd
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += switchd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

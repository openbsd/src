#	$OpenBSD: Makefile,v 1.15 2017/08/12 04:53:25 rob Exp $

SUBDIR += acme-client
SUBDIR += arp
SUBDIR += httpd
SUBDIR += ifstated
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += snmpd
SUBDIR += switchd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.29 2022/08/29 18:42:28 kn Exp $

SUBDIR += acme-client
SUBDIR += arp
SUBDIR += bgpd
SUBDIR += btrace
SUBDIR += httpd
SUBDIR += ifstated
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ocspcheck
SUBDIR += ospfd
SUBDIR += ospf6d
SUBDIR += relayd
SUBDIR += rpki-client
SUBDIR += snmpd
SUBDIR += syslogd

.if ${MACHINE} == "amd64"
SUBDIR += vmd
.endif

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
SUBDIR += installboot
.endif

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.20 2019/12/23 17:34:43 denis Exp $

SUBDIR += acme-client
SUBDIR += arp
SUBDIR += bgpd
SUBDIR += httpd
SUBDIR += ifstated
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += ospf6d
SUBDIR += rebound
SUBDIR += relayd
SUBDIR += rpki-client
SUBDIR += snmpd
SUBDIR += switchd
SUBDIR += syslogd

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
SUBDIR += vmd
.endif

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

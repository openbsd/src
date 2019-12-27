#	$OpenBSD: Makefile,v 1.22 2019/12/27 09:59:22 denis Exp $

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

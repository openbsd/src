#	$OpenBSD: Makefile,v 1.21 2019/12/26 16:46:07 denis Exp $

SUBDIR += acme-client
SUBDIR += arp
SUBDIR += bgpd
SUBDIR += httpd
SUBDIR += ifstated
SUBDIR += ldapd
SUBDIR += mtree
SUBDIR += ospfd
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

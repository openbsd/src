#	$OpenBSD: Makefile,v 1.30 2022/09/01 00:13:35 kn Exp $

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

.if ${MACHINE} == "amd64" || \
    ${MACHINE} == "i386" || \
    ${MACHINE} == "sparc64"
SUBDIR += installboot
.endif

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

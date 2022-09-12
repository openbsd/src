#	$OpenBSD: Makefile,v 1.32 2022/09/12 13:10:03 kn Exp $

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
    ${MACHINE} == "arm64" || \
    ${MACHINE} == "armv7" || \
    ${MACHINE} == "i386" || \
    ${MACHINE} == "macppc" || \
    ${MACHINE} == "octeon" || \
    ${MACHINE} == "powerpc64" || \
    ${MACHINE} == "riscv64" || \
    ${MACHINE} == "sparc64"
SUBDIR += installboot
.endif

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

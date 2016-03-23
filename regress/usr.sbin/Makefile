#	$OpenBSD: Makefile,v 1.11 2016/03/23 08:58:32 mpi Exp $

SUBDIR += arp
SUBDIR += httpd
SUBDIR += mtree
SUBDIR += ospfd
SUBDIR += relayd
SUBDIR += syslogd

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR += pkg_add
.endif

.include <bsd.subdir.mk>

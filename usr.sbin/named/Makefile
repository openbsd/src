#	$OpenBSD: Makefile,v 1.13 2002/11/14 02:54:22 millert Exp $

SUBDIR= named named-xfer ndc reload restart dig nslookup \
	host dnsquery
.if make(install)
SUBDIR+= doc/bog
.endif

VER= 4.9.11-REL

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.12 2002/11/02 02:13:18 millert Exp $

SUBDIR= named named-xfer ndc reload restart dig nslookup \
	host dnsquery
.if make(install)
SUBDIR+= doc/bog
.endif

VER= 4.9.9-REL

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.11 2001/01/28 02:18:13 niklas Exp $

SUBDIR= named named-xfer ndc reload restart dig nslookup \
	host dnsquery
.if make(install)
SUBDIR+= doc/bog
.endif

VER= 4.9.8-REL

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.10 2000/03/29 20:32:55 deraadt Exp $

SUBDIR= named named-xfer ndc reload restart dig nslookup \
	host dnsquery
.if make(install)
SUBDIR+= doc/bog
.endif

VER= 4.9.7-REL

.include <bsd.subdir.mk>

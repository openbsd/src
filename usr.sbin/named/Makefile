#	$OpenBSD: Makefile,v 1.8 1998/05/22 01:43:32 millert Exp $

SUBDIR= named named-xfer ndc reload restart dig nslookup \
	host dnsquery addr
.if make(install)
SUBDIR+= doc/bog
.endif

VER= 4.9.6-REL

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.9 1998/05/22 07:09:05 millert Exp $

SUBDIR= named named-xfer ndc reload restart dig nslookup \
	host dnsquery addr
.if make(install)
SUBDIR+= doc/bog
.endif

VER= 4.9.7-REL

.include <bsd.subdir.mk>

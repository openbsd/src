#	$OpenBSD: Makefile,v 1.63 2014/03/17 19:35:28 kettenis Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcurses libedit libevent libexpat \
	libform libfuse libkeynote libkvm libl libm libmenu \
	libocurses libossaudio libpanel libpcap librt librthread librpcsvc \
	libskey libsndio libsqlite3 libssl libusbhid libutil libwrap liby libz

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
SUBDIR+=../kerberosV/lib
.endif

.include <bsd.subdir.mk>

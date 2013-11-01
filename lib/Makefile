#	$OpenBSD: Makefile,v 1.61 2013/11/01 13:54:45 syl Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcompat libcurses \
	libedit libevent libexpat \
	libform libfuse libkeynote libkvm libl libm libmenu \
	libocurses libossaudio libpanel libpcap librthread librpcsvc \
	libskey libsndio libsqlite3 libssl libusbhid libutil libwrap liby libz

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
SUBDIR+=../kerberosV/lib
.endif

.include <bsd.subdir.mk>

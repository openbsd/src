#	$OpenBSD: Makefile,v 1.66 2014/04/11 22:51:52 miod Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcrypto libcurses libedit libevent libexpat \
	libform libfuse libkeynote libkvm libl libm libmenu \
	libocurses libossaudio libpanel libpcap librthread librpcsvc \
	libskey libsndio libsqlite3 libssl libusbhid libutil liby libz

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
SUBDIR+=../kerberosV/lib
.endif

.include <bsd.subdir.mk>

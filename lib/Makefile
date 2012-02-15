#	$OpenBSD: Makefile,v 1.58 2012/02/15 06:48:40 guenther Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcompat libcurses \
	libedit libevent libexpat \
	libform libkeynote libkvm libl libm libmenu \
	libocurses libossaudio libpanel libpcap librthread librpcsvc \
	libskey libsndio libssl libusbhid libutil libwrap liby libz

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
SUBDIR+=libgssapi libkadm5srv libkadm5clnt libkrb5
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.51 2005/04/18 16:07:31 millert Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcompat libcurses \
	libdes libedit libevent libform libkeynote libkvm libl libm libmenu \
	libocurses libossaudio libpanel libpcap libpthread libresolv librpcsvc \
	libskey libsectok libssl libusbhid libutil libwrap liby libz

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
SUBDIR+=libgssapi libkadm5srv libkadm5clnt libkrb5
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.50 2003/07/14 16:14:42 mho Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcompat libcurses libcurses++ \
	libdes libedit libevent libform libkeynote libkvm libl libm libmenu \
	libocurses libossaudio libpanel libpcap libpthread libresolv librpcsvc \
	libskey libsectok libssl libusbhid libutil libwrap liby libz

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
SUBDIR+=libgssapi libkadm5srv libkadm5clnt libkrb5
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.41 2001/06/27 05:53:16 art Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libc_r libcom_err libcompat libcurses libdes libedit \
	libform libkeynote libl libm libmenu libocurses libossaudio libpanel \
	libpcap libresolv librpcsvc libskey libsectok libssl libtelnet \
	libutil libwrap liby libz libkvm

# XXX Temporary until libcurses++ builds on mips again
.if ${MACHINE_ARCH} != "mips"
SUBDIR+=	libcurses++
.endif

.include <bsd.subdir.mk>

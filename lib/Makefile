#	$OpenBSD: Makefile,v 1.28 1998/09/11 04:08:58 millert Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libdes libedit \
	libform libl libm libmenu libocurses libossaudio libpanel libpcap \
	libresolv librpcsvc libskey libtelnet libutil libwrap liby libz

# XXX Temporarely until all i386 is able to use libkvm
.if	make(obj)
SUBDIR+=	libkvm libkvm.old
.elif	(${MACHINE} == "i386")
SUBDIR+=        libkvm.old
.else
SUBDIR+=        libkvm
.endif

.include <bsd.subdir.mk>

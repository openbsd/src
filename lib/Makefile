#	$OpenBSD: Makefile,v 1.30 1998/09/25 16:26:49 ryker Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libdes libedit \
	libform libl libm libmenu libocurses libossaudio libpanel libpcap \
	libresolv librpcsvc libskey libtelnet libutil libwrap liby libz

# XXX Temporarily until i386, pmax and mac68k are able to use libkvm
.if	make(obj)
SUBDIR+=	libkvm libkvm.old
.elif	(${MACHINE} == "i386" || ${MACHINE} == "pmax" || ${MACHINE} == "mac68k")
SUBDIR+=        libkvm.old
.else
SUBDIR+=        libkvm
.endif

.include <bsd.subdir.mk>

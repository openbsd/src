#	$OpenBSD: Makefile,v 1.32 1999/01/19 05:45:25 d Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libc_r libcom_err libcompat libcurses libdes libedit \
	libform libl libm libmenu libocurses libossaudio libpanel libpcap \
	libresolv librpcsvc libskey libssl libtelnet libutil libwrap liby libz

# XXX Temporarily until i386, pmax and mac68k are able to use libkvm
.if	make(obj)
SUBDIR+=	libkvm libkvm.old
.elif	(${MACHINE} == "i386" || ${MACHINE} == "pmax" || ${MACHINE} == "mac68k")
SUBDIR+=        libkvm.old
.else
SUBDIR+=        libkvm
.endif

.include <bsd.subdir.mk>

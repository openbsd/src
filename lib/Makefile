#	$OpenBSD: Makefile,v 1.33 1999/04/23 05:20:40 downsj Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libc_r libcom_err libcompat libcurses libdes libedit \
	libform libl libm libmenu libocurses libossaudio libpanel libpcap \
	libresolv librpcsvc libskey libssl libtelnet libutil libwrap liby libz

# XXX Temporarily until i386 and pmax are able to use libkvm
.if	make(obj)
SUBDIR+=	libkvm libkvm.old
.elif	(${MACHINE} == "i386" || ${MACHINE} == "pmax")
SUBDIR+=        libkvm.old
.else
SUBDIR+=        libkvm
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.27 1998/07/23 21:15:50 millert Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libdes libedit \
	libform libl libm libmenu libocurses libossaudio libpanel libpcap \
	libresolv librpcsvc libskey libtelnet libutil libwrap liby libz

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	make(obj)
SUBDIR+=	libkvm libkvm.old
.elif	(${MACHINE} == "alpha") || \
	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "powerpc") || \
	(${MACHINE} == "mvme88k") || \
	(${MACHINE} == "sparc") || \
	(${MACHINE} == "sun3") || \
	(${MACHINE} == "hp300")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

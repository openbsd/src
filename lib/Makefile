#	$OpenBSD: Makefile,v 1.26 1998/07/07 18:50:50 art Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libdes libedit \
	libform libl libm libmenu libocurses libossaudio libpanel libpcap \
	libresolv librpcsvc libskey libtelnet libterm libtermlib \
	libutil libwrap liby libz

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

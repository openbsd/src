#	$OpenBSD: Makefile,v 1.19 1997/02/26 16:46:25 niklas Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libedit \
	libform libl libm libmenu libocurses libpanel libpcap libresolv \
	librpcsvc libskey libss libtelnet libterm libtermlib libutil libwrap \
	liby libz

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	(${MACHINE} == "alpha") || \
	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "powerpc") || \
	(${MACHINE} == "sun3")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

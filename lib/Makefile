#	$OpenBSD: Makefile,v 1.17 1997/01/20 07:46:47 graichen Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libedit \
	libform libl libm libmenu libocurses libpanel libpcap libresolv \
	librpcsvc libskey libss libtelnet libterm libtermlib libutil liby \
	libz

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "powerpc") || \
	(${MACHINE} == "sun3")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

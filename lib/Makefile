#	$OpenBSD: Makefile,v 1.8 1996/06/18 10:07:05 deraadt Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcompat libcrypt libcurses libedit libform \
	libl libm libmenu libocurses libpanel libpcap libresolv librpcsvc \
	libskey libtelnet libterm libtermlib libutil liby

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "sun3")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.7 1996/06/02 21:47:01 tholo Exp $
#	$NetBSD: Makefile,v 1.20 1996/05/12 04:30:18 mhitch Exp $

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

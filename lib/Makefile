#	$OpenBSD: Makefile,v 1.5 1996/05/31 05:48:53 tholo Exp $
#	$NetBSD: Makefile,v 1.20 1996/05/12 04:30:18 mhitch Exp $

SUBDIR=	csu libarch libc libcompat libcrypt libcurses libedit libl \
	libm libpcap libresolv librpcsvc libskey libtelnet libterm \
	libtermlib libutil liby

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	(${MACHINE} == "amiga") || \
	(${MACHINE} == "atari") || \
	(${MACHINE} == "sun3")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

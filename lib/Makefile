#	$OpenBSD: Makefile,v 1.3 1996/05/10 12:56:50 deraadt Exp $
#	$NetBSD: Makefile,v 1.19 1996/05/05 04:37:22 gwr Exp $
#	from: @(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

SUBDIR=	csu libarch libc libcompat libcrypt libcurses libedit libl \
	libm libpcap libresolv librpcsvc libskey libtelnet libterm libutil liby

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	(${MACHINE} == "atari") || \
	(${MACHINE} == "sun3")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

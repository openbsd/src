#	$OpenBSD: Makefile,v 1.2 1996/03/19 23:14:49 niklas Exp $
#	$NetBSD: Makefile,v 1.18 1996/03/16 10:23:29 leo Exp $
#	from: @(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

SUBDIR=	csu libarch libc libcompat libcrypt libcurses libedit libl \
	libm libpcap libresolv librpcsvc libskey libtelnet libterm libutil liby

# XXX Temporarely until all ports are able to use libkvm (leo)
.if	(${MACHINE} == "atari")
SUBDIR+=        libkvm
.else
SUBDIR+=        libkvm.old
.endif

.include <bsd.subdir.mk>

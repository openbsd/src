#	$OpenBSD: Makefile,v 1.38 2000/11/19 21:27:13 millert Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcom_err libcompat libcurses libdes libedit \
	libform libkeynote libl libm libmenu libocurses libossaudio libpanel \
	libpcap libresolv librpcsvc libskey libssl libtelnet libutil libwrap \
	liby libz

# XXX Temporary until libc_r builds on alpha again
.if ${MACHINE} != "alpha"
SUBDIR+=        libc_r
.endif

# XXX Temporary until libcurses++ builds on mips again
.if ${MACHINE_ARCH} != "mips"
SUBDIR+=	libcurses++
.endif

# XXX Temporarily until pmax is able to use libkvm
.if	make(obj)
SUBDIR+=	libkvm libkvm.old
.elif	(${MACHINE} == "pmax")
SUBDIR+=        libkvm.old
.else
SUBDIR+=        libkvm
.endif

.include <bsd.subdir.mk>

#	$OpenBSD: Makefile,v 1.16 1997/01/19 22:41:37 graichen Exp $
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

# XXX Temporarely until the mips ld can handle __warn_references
.if (${MACHINE_ARCH} == "mips")
CFLAGS+=-DNO_WARN_REFERENCES
.endif

.include <bsd.subdir.mk>

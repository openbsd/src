#	$OpenBSD: Makefile,v 1.1.1.1 2000/06/13 03:34:09 rahnds Exp $

CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}

SHLIB_MAJOR=1
SHLIB_MINOR=0

SRCS=	libdl.c
LIB=	dl

NOPROFILE=yes

BINDIR=/usr/libexec
INSTALL_STRIP=



.include <bsd.lib.mk>

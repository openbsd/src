#	$OpenBSD: Makefile,v 1.2 2001/07/22 13:35:25 espie Exp $

CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}

SRCS=	libdl.c
LIB=	dl

NOPROFILE=yes

BINDIR=/usr/libexec
INSTALL_STRIP=

.include <bsd.lib.mk>

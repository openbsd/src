#	$OpenBSD: Makefile,v 1.4 1997/01/19 12:29:23 mickey Exp $

LKM=	ap
SRCS+=	aperture.c lkm.c version.c
MAN=	ap.4
MLINKS=	ap.4 xf86.4

.ifmake !(install)
SUBDIR=	aptest
.endif

.include <bsd.lkm.mk>

#	$OpenBSD: Makefile,v 1.3 1996/06/03 04:02:22 mickey Exp $

LKM=	ap
SRCS+=	aperture.c lkm.c version.c
NOMAN=	noman

.ifmake !(install)
SUBDIR=	aptest
.endif

.include <bsd.lkm.mk>

# $OpenBSD: Makefile,v 1.3 2000/03/30 08:03:24 hin Exp $

LIB	= kafs

KRBDEPTH=../..
.include <../../Makefile.inc>

INCLUDE_FILES	= ${KRBSRC}/lib/kafs/kafs.h

CFLAGS += -I${KRBSRC}/lib/kafs

.PATH:	${KRBSRC}/lib/kafs ${KRBSRC}/man

SRCS=	afssys_openbsd.c \
	afskrb.c \
	$(afskrb5_c) \
	common.c \
	kafs_locl.h \
	afssysdefs.h

MAN=	kafs.3

.include <bsd.lib.mk>

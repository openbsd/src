# $OpenBSD: Makefile,v 1.2 2000/03/02 10:17:28 art Exp $

LIB	= kafs

KRBDEPTH=../..
.include <../../Makefile.inc>

INCLUDE_FILES	= ${KRBSRC}/lib/kafs/kafs.h

CFLAGS += -I${KRBSRC}/lib/kafs

.PATH:	${KRBSRC}/lib/kafs

SRCS=	afssys_openbsd.c \
	afskrb.c \
	$(afskrb5_c) \
	common.c \
	kafs_locl.h \
	afssysdefs.h

.include <bsd.lib.mk>

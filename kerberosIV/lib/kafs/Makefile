# $OpenBSD: Makefile,v 1.4 2000/04/01 22:45:27 deraadt Exp $

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
MLINKS=	kafs.3 k_hasafs.3 \
	kafs.3 k_pioctl.3 \
	kafs.3 k_unlog.3 \
	kafs.3 k_sepag.3 \
	kafs.3 k_afs_cell_of_file.3 \
	kafs.3 krb_afslog.3 \
	kafs.3 krb_afslog_uid.3

.include <bsd.lib.mk>

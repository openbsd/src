# $OpenBSD: Makefile,v 1.4 2003/05/11 03:47:55 hin Exp $

LIB	= kafs

KRBDEPTH=../..
.include "../../Makefile.inc"

INCLUDE_FILES	= ${KRB5SRC}/lib/kafs/kafs.h

CFLAGS += -I${KRB5SRC}/lib/kafs -DROKEN_RENAME

.PATH:	${KRB5SRC}/lib/kafs ${KRB5SRC}/man

SRCS=	afssys_openbsd.c \
	afskrb.c \
	afskrb5.c \
	common.c \
	kafs_locl.h \
	afssysdefs.h

MAN=	kafs.3
MLINKS=	kafs.3 k_hasafs.3 \
	kafs.3 k_pioctl.3 \
	kafs.3 k_unlog.3 \
	kafs.3 k_setpag.3 \
	kafs.3 k_afs_cell_of_file.3 \
	kafs.3 krb_afslog.3 \
	kafs.3 krb_afslog_uid.3

.include <bsd.lib.mk>

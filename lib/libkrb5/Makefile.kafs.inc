# $OpenBSD: Makefile.kafs.inc,v 1.2 2003/05/15 04:16:10 hin Exp $

INCLUDE_FILES+=	${KRB5SRC}/lib/kafs/kafs.h

CFLAGS += -I${KRB5SRC}/lib/kafs

.PATH:	${KRB5SRC}/lib/kafs ${KRB5SRC}/man

SRCS+=	afssys_openbsd.c \
	afskrb5.c \
	kafs_common.c \
	kafs_locl.h \
	afssysdefs.h

MAN+=	kafs.3
MLINKS+=kafs.3 k_hasafs.3 \
	kafs.3 k_pioctl.3 \
	kafs.3 k_unlog.3 \
	kafs.3 k_setpag.3 \
	kafs.3 k_afs_cell_of_file.3 \
	kafs.3 krb_afslog.3 \
	kafs.3 krb_afslog_uid.3

kafs_common.c:	${KRB5SRC}/lib/kafs/common.c
	ln -sf ${KRB5SRC}/lib/kafs/common.c kafs_common.c


# $OpenBSD: Makefile.kafs.inc,v 1.6 2006/04/29 12:35:56 biorn Exp $

DIR_KAFS=	${KRB5SRC}/lib/kafs
HDR_KAFS=	kafs.h

CFLAGS += -I${DIR_KAFS}

.PATH:	${DIR_KAFS} ${KRB5SRC}/man

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
	kafs.3 kafs_set_verbose.3 \
	kafs.3 kafs_settoken_rxkad.3 \
	kafs.3 kafs_settoken.3 \
	kafs.3 krb_afslog.3 \
	kafs.3 krb_afslog_uid.3 \
	kafs.3 kafs_settoken5.3 \
	kafs.3 krb5_afslog.3 \
	kafs.3 krb5_afslog_uid.3

kafs_common.c:	${DIR_KAFS}/common.c
	ln -sf ${DIR_KAFS}/common.c kafs_common.c
CLEANFILES+=kafs_common.c

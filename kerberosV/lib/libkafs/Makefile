# $OpenBSD: Makefile,v 1.1 2013/08/10 09:07:39 ajacoutot Exp $

LIB=	kafs
INCS=	kafs.h
MPAGES=	kafs.3
MSRC=	${KRB5DIR}/lib/kafs

MLINKS=	kafs.3 k_afs_cell_of_file.3 \
	kafs.3 k_hasafs.3 \
	kafs.3 k_pioctl.3 \
	kafs.3 k_setpag.3 \
	kafs.3 k_unlog.3 \
	kafs.3 kafs_set_verbose.3 \
	kafs.3 kafs_settoken.3 \
	kafs.3 kafs_settoken5.3 \
	kafs.3 kafs_settoken_rxkad.3 \
	kafs.3 krb5_afslog.3 \
	kafs.3 krb5_afslog_uid.3 \
	kafs.3 krb_afslog.3 \
	kafs.3 krb_afslog_uid.3

SRCS=	afssys.c \
	afskrb5.c \
	common.c

CPPFLAGS=	-I${KRB5DIR}/lib/kafs \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/kafs

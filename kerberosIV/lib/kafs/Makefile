# $OpenBSD: Makefile,v 1.7 2001/06/05 10:10:58 hin Exp $

LIB	= kafs

KRBDEPTH=../..
.include "../../Makefile.inc"

INCLUDE_FILES	= ${KRBSRC}/lib/kafs/kafs.h

CFLAGS += -I${KRBSRC}/lib/kafs

.PATH:	${KRBSRC}/lib/kafs ${KRBSRC}/man

SRCS=	afssys_openbsd.c \
	afskrb.c \
	common.c \
	kafs_locl.h \
	afssysdefs.h

.include <bsd.own.mk> # for KERBEROS5

.if (${KERBEROS5:L} == "yes")
SRCS+=	afskrb5.c
CFLAGS+=-DKRB5 -I${DESTDIR}/usr/include/kerberosV
.endif

MAN=	kafs.3
MLINKS=	kafs.3 k_hasafs.3 \
	kafs.3 k_pioctl.3 \
	kafs.3 k_unlog.3 \
	kafs.3 k_setpag.3 \
	kafs.3 k_afs_cell_of_file.3 \
	kafs.3 krb_afslog.3 \
	kafs.3 krb_afslog_uid.3

.include <bsd.lib.mk>

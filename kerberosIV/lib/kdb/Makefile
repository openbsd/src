# $OpenBSD: Makefile,v 1.2 2001/01/09 02:41:17 angelos Exp $

LIB	= kdb

KRBDEPTH=../..
.include "../../Makefile.inc"

INCLUDE_FILES	= ${KRBSRC}/lib/kdb/krb_db.h

.PATH:	${KRBSRC}/lib/kdb

CFLAGS	+= -I${KRBSRC}/lib/kdb

SRCS=	krb_cache.c \
	krb_kdb_utils.c \
	copykey.c \
	krb_lib.c \
	krb_dbm.c \
	print_princ.c

.include <bsd.lib.mk>

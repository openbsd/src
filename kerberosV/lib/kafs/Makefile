# $OpenBSD: Makefile,v 1.5 2003/05/14 01:44:56 hin Exp $

LIB	= kafs

KRBDEPTH=../..
.include "../../Makefile.inc"

INCLUDE_FILES	= ${KRB5SRC}/lib/kafs/kafs.h

CFLAGS += -I${KRB5SRC}/lib/kafs -DROKEN_RENAME

.PATH:	${KRB5SRC}/lib/kafs ${KRB5SRC}/man

SRCS=	dummy.c

MAN=

.include <bsd.lib.mk>

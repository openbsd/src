# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:17 angelos Exp $

LIB	= acl

KRBDEPTH=../..
.include "../../Makefile.inc"

INCLUDE_FILES	= ${KRBSRC}/lib/acl/acl.h

.PATH:	${KRBSRC}/lib/acl

CFLAGS	+= -I${KRBSRC}/lib/acl

SRCS=	acl_files.c

MAN=	acl_check.3
MLINKS=	acl_check.3 acl_canonicalize_principal.3 \
	acl_check.3 acl_exact_match.3 \
	acl_check.3 acl_add.3 acl_check.3 acl_delete.3 \
	acl_check.3 acl_initialize.3

.include <bsd.lib.mk>

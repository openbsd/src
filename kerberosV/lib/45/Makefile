# $OpenBSD: Makefile,v 1.1 2001/05/25 08:18:51 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

LIB=	45
SRCS=	get_ad_tkt.c mk_req.c

.PATH:	${KRB5SRC}/lib/45
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I../krb5 -I../asn1

.include <bsd.lib.mk>
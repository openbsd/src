# $OpenBSD: Makefile,v 1.3 2003/05/11 20:54:39 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

LIB=	45
SRCS=	get_ad_tkt.c mk_req.c

.PATH:	${KRB5SRC}/lib/45
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I../krb5 -I../asn1

.include <bsd.lib.mk>

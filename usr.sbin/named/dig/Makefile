#	$OpenBSD: Makefile,v 1.3 1997/03/12 10:41:48 downsj Exp $
#	$NetBSD: Makefile,v 1.2 1996/03/31 13:55:34 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:45 vixie Exp 

.PATH:	${.CURDIR}/../nslookup \
	${.CURDIR}/../libresolv \
	${.CURDIR}/../man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c base64.c gethnamaddr.c res_comp.c \
	res_debug.c res_init.c res_mkquery.c res_query.c res_send.c \
	inet_ntop.c inet_pton.c
CFLAGS+=${INCLUDE} ${CONFIG} -DDEBUG

.include <bsd.prog.mk>
.include "../../Makefile.inc"

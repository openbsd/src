#	$OpenBSD: Makefile,v 1.6 1998/11/20 11:19:01 d Exp $
#	$NetBSD: Makefile,v 1.2 1996/03/31 13:55:34 mrg Exp $
# from: $Id: Makefile,v 8.1 1994/12/15 06:23:45 vixie Exp 

.PATH:	${.CURDIR}/../nslookup \
	${.CURDIR}/../../../lib/libc/net \
	${.CURDIR}/../man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c base64.c gethostnamadr.c res_comp.c \
	res_debug.c res_init.c res_mkquery.c res_query.c res_send.c \
	res_random.c inet_ntop.c inet_pton.c
INCLUDE+=-I${.CURDIR}/../../../lib/libc/include
INCLUDE+=-I${.CURDIR}/..
CFLAGS+=${INCLUDE} ${CONFIG} -DDEBUG

.include <bsd.prog.mk>
.include "../../Makefile.inc"

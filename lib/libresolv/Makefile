#	from: @(#)Makefile	5.1 (Berkeley) 6/5/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:43:09 deraadt Exp $

LIB=resolv
CFLAGS+=-DDEBUG -DLIBC_SCCS

.PATH:	${.CURDIR}/../libc/net

SRCS=gethostnamadr.c res_mkquery.c res_query.c res_send.c

.include <bsd.lib.mk>

#	$OpenBSD: Makefile,v 1.4 2013/01/26 09:37:24 gilles Exp $

.PATH:		${.CURDIR}/..

CFLAGS	  = -DNO_IO
LIB	  = smtpdfilter
SRCS	  = filter_api.c dict.c tree.c util.c iobuf.c log.c
DEBUGLIBS = no
NOPROFILE = yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>
.include <bsd.lib.mk>
